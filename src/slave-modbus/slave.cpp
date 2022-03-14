#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <iterator>
#include <list>
#include <nlohmann/json.hpp>

#include <modbus/modbus.h>
#include <spider_slave.h>

using string = std::string;
using json = nlohmann::json;

static int exit_flag;
static pid_t ppid;
static string app_json_file;
static string address;
static string credential;

struct channel {
    string id;

    string type;

    // tcp
    string ipaddr;
    int port;
    long timeout;

    // rtu
    int com;
    int baud_rate;
    string parity;
    int data_bit;
    int stop_bit;
    int modbus_slave_id;

    modbus_t *conn;
};
static std::map<string, struct channel> channels;

static struct param_desc channel_desc[] = {
    INIT_PD_SELECT("type", "通道类型", NULL, "tcp rtu", "tcp"),
    INIT_PD_TCP("type=tcp", 502),
    INIT_PD_SERIAL("type=rtu"),
    INIT_PD_NUMBER("modbus_slave_id", "站号", NULL, 1, 1, 32),
    INIT_PD_NUMBER("timeout", "超时时间", NULL, 1000, 100, 3000),
    INIT_PD_NONE(),
};

static struct param_desc point_desc[] = {
    INIT_PD_NUMBER("address", "地址", NULL, 0, 0, USHRT_MAX),
    INIT_PD_NUMBER("quantity", "数量", NULL, 1, 1, USHRT_MAX),
    INIT_PD_SELECT("function_code", "功能码", NULL,
                   "1(读线圈状态) 2(读离散输入状态) 3(读保持寄存器)" \
                   "4(读输入寄存器) 5(写单个线圈) 6(写单个保持寄存器)" \
                   "15(写多个线圈) 16(写多个保持寄存器)",
                   "3(读保持寄存器)"),
    INIT_PD_SELECT("value_type", "数据类型", NULL,
                   "1(整型) 2(无符号整型) 3(字符串) 4(浮点数)",
                   "1(整型)"),
    INIT_PD_NONE(),
};

static struct slave_metadata emulator_slave_metadata = {
    .channel_desc = channel_desc,
    .point_desc = point_desc,
};

static void on_slave_metadata(struct slave_metadata **meta)
{
    *meta = &emulator_slave_metadata;
}

static int on_channel_create(const char *id, const char *param)
{
    struct channel chn;

    try {
        json j = json::parse(param);
        chn.id = id;
        chn.type = j["type"];
        chn.modbus_slave_id = j["modbus_slave_id"];
        chn.timeout = j["timeout"];
        if (chn.type == "tcp") {
            chn.ipaddr = j["ipaddr"];
            chn.port = j["port"];
        } else if (chn.type == "rtu") {
            chn.com = atoi(j["com"].get<string>().c_str());
            chn.baud_rate = atoi(j["baud_rate"].get<string>().c_str());
            chn.parity = j["parity"];
            chn.data_bit = atoi(j["data_bit"].get<string>().c_str());
            chn.stop_bit = atoi(j["stop_bit"].get<string>().c_str());
        } else {
            return -1;
        }
    } catch (json::exception &ex) {
        return -1;
    }

    if (chn.type == "tcp") {
        chn.conn = modbus_new_tcp(chn.ipaddr.c_str(), chn.port);
    } else {
        char com_name[32] = {0};
#ifdef __CYGWIN__
        sprintf(com_name, "COM%d", chn.com);
#else
        sprintf(com_name, "/dev/ttyS%d", chn.com);
#endif
        chn.conn = modbus_new_rtu(
            com_name,
            chn.baud_rate,
            chn.parity.c_str()[0],
            chn.data_bit,
            chn.stop_bit);
    }
    assert(modbus_set_slave(chn.conn, chn.modbus_slave_id) == 0);
    assert(modbus_set_response_timeout(
               chn.conn, chn.timeout / 1000, (chn.timeout % 1000) * 1000) == 0);
    assert(modbus_set_byte_timeout(
               chn.conn, chn.timeout / 1000, (chn.timeout % 1000) * 1000) == 0);
    if (modbus_connect(chn.conn) == -1) {
        fprintf(stderr, "[modbus]: %s\n", modbus_strerror(errno));
        modbus_close(chn.conn);
        modbus_free(chn.conn);
        return -1;
    }

    channels.insert(std::make_pair(chn.id, chn));
    return 0;
}

static void on_channel_delete(const char *id)
{
    auto chn_it = channels.find(id);
    if (chn_it == channels.end())
        return;

    modbus_close(chn_it->second.conn);
    modbus_free(chn_it->second.conn);
    channels.erase(chn_it);
}

static int strip_parenthesis(char *buf)
{
    char *begin = strchr(buf, '(');
    char *end = strchr(buf, ')');

    if (!begin || !end || begin >= end || end != buf + strlen(buf) - 1)
        return -1;

    *begin = 0;
    return 0;
}

static int on_point_read(
    const char *channel_id, const char *param, char *json_buf, size_t size)
{
    auto chn_it = channels.find(channel_id);
    if (chn_it == channels.end())
        return -1;

    modbus_t *conn = chn_it->second.conn;

    /*
     * 读取功能码
     * 1 -- Coils 读线圈
     * 2 -- Discrete Inputs 读输入状态
     * 3 -- Holding Registers 保持寄存器
     * 4 -- Input Registers 输入寄存器
     */

    int address, quantity, function_code, value_type;

    try {
        json j = json::parse(param);
        address = j["address"];
        quantity = j["quantity"];
        string _code = j["function_code"];
        string _type = j["value_type"];

        char *tmp_code = strdup(_code.c_str());
        strip_parenthesis(tmp_code);
        function_code = atoi(tmp_code);
        free(tmp_code);

        char *tmp_type = strdup(_type.c_str());
        strip_parenthesis(tmp_type);
        value_type = atoi(tmp_type);
        free(tmp_type);
    } catch (json::exception &ex) {
        return -1;
    }

    if (address < 0 || address > 0xFFFF)
        return -1;

    int rc = 0;
    void *buf = NULL;
    size_t buf_size = 0;
    //modbus_rtu_lock(chn.conn);
    if (1 == function_code) {
        buf_size = quantity;
        buf = calloc(1, buf_size);
        rc = modbus_read_bits(conn, address, quantity, (uint8_t *)buf);
    } else if (2 == function_code) {
        buf_size = quantity;
        buf = calloc(1, buf_size);
        rc = modbus_read_input_bits(
            conn, address, quantity, (uint8_t *)buf);
    } else if (3 == function_code) {
        buf_size = quantity << 1;
        buf = calloc(1, buf_size);
        rc = modbus_read_registers(
            conn, address, quantity, (uint16_t *)buf);
    } else if (4 == function_code) {
        buf_size = quantity << 1;
        buf = calloc(1, buf_size);
        rc = modbus_read_input_registers(
            conn, address, quantity, (uint16_t *)buf);
    } else {
        //modbus_rtu_unlock(conn);
        fprintf(stderr, "[modbus]: error function_code [%d]\n",
                function_code);
        return -1;
    }
    //modbus_rtu_unlock(conn);
    if (rc == -1) {
        fprintf(stderr, "[modbus]: %s\n", modbus_strerror(errno));
        modbus_close(conn);
        if (modbus_connect(conn) == -1) {
            fprintf(stderr, "[modbus]: %s\n", modbus_strerror(errno));
        }
        free(buf);
        return -1;
    }

    json cnt = {{"ts", time(NULL)}};

    if (1 == value_type) {
        if (buf_size == 2)
            cnt["value"] = *(short *)buf;
        else if (buf_size == 4)
            cnt["value"] = *(int *)buf;
        else if (buf_size == 8)
            cnt["value"] = *(long *)buf;
        else {
            free(buf);
            return -1;
        }
    } else if (2 == value_type) {
        if (buf_size == 2)
            cnt["value"] = *(unsigned short *)buf;
        else if (buf_size == 4)
            cnt["value"] = *(unsigned int *)buf;
        else if (buf_size == 8)
            cnt["value"] = *(unsigned long *)buf;
        else {
            free(buf);
            return -1;
        }
    } else if (3 == value_type) {
        cnt["value"] = string((char *)buf, buf_size);
    } else if (4 == value_type) {
        if (buf_size == 4)
            cnt["value"] = *(float *)buf;
        else if (buf_size == 8)
            cnt["value"] = *(double *)buf;
        else {
            free(buf);
            return -1;
        }
    }
    free(buf);

    string dump = cnt.dump();
    assert(dump.size() < size);
    strcpy(json_buf, dump.c_str());
    return dump.size();
}

static int on_point_write(
    const char *channel_id, const char *param, const char *json_buf)
{
    return strlen(json_buf);
}

static struct slave_operations ops = {
    .slave_metadata = on_slave_metadata,
    .channel_create = on_channel_create,
    .channel_delete = on_channel_delete,
    .point_read = on_point_read,
    .point_write = on_point_write,
};

static void signal_handler(int sig)
{
    exit_flag = 1;
}

static int parse_option(int argc, char *argv[])
{
    int c;
    while ((c = getopt (argc, argv, "j:a:s:c:")) != -1)
        switch (c) {
        case 'j':
            app_json_file = optarg;
            break;
        case 'a':
            address = optarg;
            break;
        case 'c':
            credential = optarg;
            break;
        case '?':
            if (optopt == 'j' || optopt == 'a' ||
                optopt == 's' || optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
            abort ();
        }

    if (app_json_file.empty() || address.empty() || credential.empty()) {
        fprintf(stderr, "[modbus]: Invalid arguments\n");
        for (int i = 0; i < argc; i++)
            fprintf(stderr, "%d: %s\n", i, argv[i]);
        abort();
    }

    return 0;
}

int main(int argc, char *argv[])
{
    ppid = getppid();
    parse_option(argc, argv);

    std::ifstream ifs(app_json_file);
    json conf = json::parse(ifs);
    ifs.close();

    struct slave_info info;
    info.address = address.c_str();
    info.credential = credential.c_str();
    string appid = conf["id"];
    info.appid = appid.c_str();
    string appname = conf["name"];
    info.appname = appname.c_str();
    string appdesc = conf["desc"];
    info.appdesc = appdesc.c_str();
    string version = conf["version"];
    info.version = version.c_str();
    struct spider_slave *spd = spider_slave_new(&info, &ops);

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, signal_handler);

    while (exit_flag == 0 && ppid == getppid())
        spider_slave_loop(spd, 1000);

    spider_slave_destroy(spd);
    return 0;
}
