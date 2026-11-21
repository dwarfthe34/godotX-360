#include <stdio.h>

#include "main/main.h"
#include "os_xbox360xsdk.h"

#include "xsdk/log.h"

int main(int argc, char *argv[]) {
	xsdk_log_write("xbox360xsdk: main() entered");

	OS_Xbox360Xsdk os;
	xsdk_log_write("xbox360xsdk: OS constructed");

	const char *exec_path = argc > 0 && argv[0] ? argv[0] : "game.xex";
	const int argument_count = argc > 0 ? argc - 1 : 0;
	char **arguments = argc > 0 ? &argv[1] : nullptr;

	xsdk_log_write("xbox360xsdk: Main::setup exec_path=%s", exec_path);
	Error error = Main::setup(exec_path, argument_count, arguments);
	if (error != OK) {
		if (error == ERR_HELP) {
			return 0;
		}
		xsdk_log_write("xbox360xsdk: Main::setup failed: %d", (int)error);
		return 255;
	}


	xsdk_log_write("xbox360xsdk: Main::start");
	if (Main::start()) {
		xsdk_log_write("xbox360xsdk: entering OS::run");
		os.run();
	}

	xsdk_log_write("xbox360xsdk: Main::cleanup");
	Main::cleanup();
	return os.get_exit_code();
}
