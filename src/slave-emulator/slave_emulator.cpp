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
#include <spider_slave.h>

using string = std::string;
using json = nlohmann::json;
using json_pointer = nlohmann::json::json_pointer;

static int exit_flag;
static pid_t ppid;
static string app_json_file;
static string address;
static string credential;
static json cache;

static struct param_desc channel_desc[] = {
	INIT_PD_SELECT("type", "通道类型", NULL, "net serial", "net"),
	INIT_PD_TCP("type=net", 5200),
	INIT_PD_SERIAL("type=serial"),
	INIT_PD_NONE(),
};

static struct param_desc point_desc[] = {
	INIT_PD_TEXT("jsonpath", "json路径", NULL, "", 0, 256),
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
	return 0;
}

static void on_channel_delete(const char *id)
{
}

static int on_point_read(const char *channel_id, const char *param,
                         char *json_buf, size_t size)
{
	try {
		json cnt = json::parse(param);
		string value_path = cnt["jsonpath"];

		json j;
		j["ts"] = time(NULL);
		j["value"] = cache.at(json_pointer(value_path));
		strcpy(json_buf, j.dump().c_str());
		return strlen(j.dump().c_str());
	} catch (json::exception &ex) {
		fprintf(stderr, "%s", ex.what());
		return -1;
	}
}

static int on_point_write(const char *channel_id, const char *param,
                          const char *json_buf)
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
				fprintf(stderr,
				        "Option -%c requires an argument.\n",
				        optopt);
			else if (isprint (optopt))
				fprintf(stderr,
				        "Unknown option `-%c'.\n",
				        optopt);
			else
				fprintf(stderr,
				        "Unknown option character `\\x%x'.\n",
				        optopt);
			return 1;
		default:
			abort ();
		}

	if (app_json_file.empty() || address.empty() || credential.empty()) {
		fprintf(stderr, "slave_emulator: Invalid arguments\n");
		for (int i = 0; i < argc; i++)
			fprintf(stderr, "%d: %s\n", i, argv[i]);
		abort();
	}

	return 0;
}

static double trim_integer(double f)
{
	return (double)(int)((f - (int)f) * 10000) / 10000;
}

static void update_cache()
{
	cache["inc"]["0"] = cache["inc"]["0"].get<int>() + rand() % 3;
	cache["inc"]["1"] = cache["inc"]["1"].get<int>() + rand() % 5;
	cache["inc"]["2"] = cache["inc"]["2"].get<int>() + rand() % 8;
	cache["inc"]["3"] = cache["inc"]["3"].get<int>() + rand() % 13;
	cache["inc"]["4"] = cache["inc"]["4"].get<int>() + rand() % 21;

	cache["inc"]["percent"]["0"] = trim_integer(
		cache["inc"]["percent"]["0"].get<double>() + rand()%3*0.0001);
	cache["inc"]["percent"]["1"] = trim_integer(
		cache["inc"]["percent"]["1"].get<double>() + rand()%5*0.0001);
	cache["inc"]["percent"]["2"] = trim_integer(
		cache["inc"]["percent"]["2"].get<double>() + rand()%8*0.0001);
	cache["inc"]["percent"]["3"] = trim_integer(
		cache["inc"]["percent"]["3"].get<double>() + rand()%13*0.0001);
	cache["inc"]["percent"]["4"] = trim_integer(
		cache["inc"]["percent"]["4"].get<double>() + rand()%21*0.0001);

	static int direct = 1;

	direct = 0 - direct;
	cache["wave"]["0"] = cache["wave"]["0"].get<int>() + rand() % 3 * direct;
	cache["wave"]["1"] = cache["wave"]["1"].get<int>() + rand() % 3 * direct;
	cache["wave"]["2"] = cache["wave"]["2"].get<int>() + rand() % 5 * direct;
	cache["wave"]["3"] = cache["wave"]["3"].get<int>() + rand() % 5 * direct;
	cache["wave"]["4"] = cache["wave"]["4"].get<int>() + rand() % 8 * direct;
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

	cache["0"] = 0;
	cache["1"] = 1;
	cache["2"] = 2;
	cache["3"] = 3;
	cache["4"] = 4;
	cache["percent"]["10"] = 0.10;
	cache["percent"]["20"] = 0.20;
	cache["percent"]["30"] = 0.30;
	cache["percent"]["40"] = 0.40;
	cache["percent"]["50"] = 0.50;
	cache["percent"]["60"] = 0.60;
	cache["percent"]["70"] = 0.70;
	cache["percent"]["80"] = 0.80;
	cache["percent"]["90"] = 0.90;
	cache["percent"]["91"] = 0.91;
	cache["percent"]["92"] = 0.92;
	cache["percent"]["93"] = 0.93;
	cache["percent"]["94"] = 0.94;
	cache["percent"]["95"] = 0.95;
	cache["percent"]["96"] = 0.96;
	cache["percent"]["97"] = 0.97;
	cache["percent"]["98"] = 0.98;
	cache["percent"]["99"] = 0.99;
	cache["percent"]["100"] = 1;
	cache["inc"]["0"] = 0;
	cache["inc"]["1"] = 0;
	cache["inc"]["2"] = 0;
	cache["inc"]["3"] = 0;
	cache["inc"]["4"] = 0;
	cache["inc"]["percent"]["0"] = 0.01;
	cache["inc"]["percent"]["1"] = 0.01;
	cache["inc"]["percent"]["2"] = 0.01;
	cache["inc"]["percent"]["3"] = 0.01;
	cache["inc"]["percent"]["4"] = 0.01;
	cache["wave"]["0"] = 30;
	cache["wave"]["1"] = 50;
	cache["wave"]["2"] = 70;
	cache["wave"]["3"] = 90;
	cache["wave"]["4"] = 110;
	srand(time(NULL));

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, signal_handler);

	while (exit_flag == 0 && ppid == getppid()) {
		spider_slave_loop(spd, 1000);
		if (time(NULL) % 5 == 0)
			update_cache();
	}

	spider_slave_destroy(spd);
	return 0;
}
