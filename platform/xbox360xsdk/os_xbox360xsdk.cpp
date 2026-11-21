#include "os_xbox360xsdk.h"

#include "core/os/os.h"
#include "drivers/dummy/rasterizer_dummy.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/xsdk/rasterizer_xsdk.h"
#include "main/main.h"
#include "servers/audio_server.h"
#include "servers/visual/visual_server_raster.h"

#include "xsdk/kernel.h"
#include "xsdk/log.h"

#include <time.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t _monotonic_usec() {
	struct timespec ts = {};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

OS_Xbox360Xsdk::OS_Xbox360Xsdk() {
	main_loop = nullptr;
	input = nullptr;
	visual_server = nullptr;
	timebase_start = _monotonic_usec();
}

void OS_Xbox360Xsdk::initialize_core() {
	timebase_start = _monotonic_usec();

	// xsdk is newlib-based and provides libc fopen(), so the Unix file access
	// drivers work, backed by xsdk's devices (game:, HDD, USB).
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);
}

Error OS_Xbox360Xsdk::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {
	current_videomode = p_desired;

	RasterizerXsdk::make_current();

	visual_server = memnew(VisualServerRaster);
	visual_server->init();
	xsdk_log_write("xsdk os: visual_server init done");

	AudioDriverManager::initialize(p_audio_driver);
	xsdk_log_write("xsdk os: audio init done");

	input = memnew(InputDefault);
	input->joy_connection_changed(0, true, "Xbox 360 Controller", "XSDK");
	xsdk_log_write("xsdk os: initialize done");
	return OK;
}

void OS_Xbox360Xsdk::finalize() {
	if (input) {
		memdelete(input);
		input = nullptr;
	}
	if (visual_server) {
		visual_server->finish();
		memdelete(visual_server);
		visual_server = nullptr;
	}
	delete_main_loop();
}

void OS_Xbox360Xsdk::finalize_core() {
}

void OS_Xbox360Xsdk::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
	// InputDefault needs the MainLoop to deliver parsed events to the SceneTree.
	// X11/Windows/UWP all do this; the Xbox 360 port was missing it.
	if (input) {
		input->set_main_loop(p_main_loop);
	}
}

void OS_Xbox360Xsdk::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
		main_loop = nullptr;
	}
}

bool OS_Xbox360Xsdk::_check_internal_feature_support(const String &) {
	return false;
}

String OS_Xbox360Xsdk::get_name() const {
	return "Xbox 360";
}

void OS_Xbox360Xsdk::alert(const String &p_alert, const String &p_title) {
	xsdk_log_write("%s: %s", p_title.utf8().get_data(), p_alert.utf8().get_data());
}

String OS_Xbox360Xsdk::get_stdin_string() {
	return String();
}

Point2 OS_Xbox360Xsdk::get_mouse_position() const {
	return Point2();
}

int OS_Xbox360Xsdk::get_mouse_button_state() const {
	return 0;
}

void OS_Xbox360Xsdk::set_window_title(const String &) {
}

void OS_Xbox360Xsdk::set_video_mode(const VideoMode &p_video_mode, int) {
	current_videomode = p_video_mode;
}

OS::VideoMode OS_Xbox360Xsdk::get_video_mode(int) const {
	return current_videomode;
}

void OS_Xbox360Xsdk::get_fullscreen_mode_list(List<VideoMode> *, int) const {
}

int OS_Xbox360Xsdk::get_current_video_driver() const {
	return 0;
}

int OS_Xbox360Xsdk::get_video_driver_count() const {
	return 1;
}

const char *OS_Xbox360Xsdk::get_video_driver_name(int p_driver) const {
	return p_driver == 0 ? "XSDK" : "";
}

Error OS_Xbox360Xsdk::get_entropy(uint8_t *r_buffer, int p_bytes) {
	return getentropy(r_buffer, p_bytes) == 0 ? OK : FAILED;
}

Size2 OS_Xbox360Xsdk::get_window_size() const {
	return Size2(current_videomode.width, current_videomode.height);
}

bool OS_Xbox360Xsdk::can_draw() const {
	return true;
}

Error OS_Xbox360Xsdk::execute(const String &, const List<String> &, bool, ProcessID *, String *, int *, bool, Mutex *, bool) {
	return ERR_UNAVAILABLE;
}

Error OS_Xbox360Xsdk::kill(const ProcessID &) {
	return ERR_UNAVAILABLE;
}

bool OS_Xbox360Xsdk::is_process_running(const ProcessID &) const {
	return false;
}

bool OS_Xbox360Xsdk::has_environment(const String &) const {
	return false;
}

String OS_Xbox360Xsdk::get_environment(const String &) const {
	return String();
}

bool OS_Xbox360Xsdk::set_environment(const String &, const String &) const {
	return false;
}

MainLoop *OS_Xbox360Xsdk::get_main_loop() const {
	return main_loop;
}

OS::Date OS_Xbox360Xsdk::get_date(bool) const {
	Date date = {};
	time_t t = ::time(nullptr);
	struct tm buf = {};
	struct tm *tm = localtime_r(&t, &buf);
	if (tm) {
		date.year = tm->tm_year + 1900;
		date.month = (Month)(tm->tm_mon + 1);
		date.day = tm->tm_mday;
		date.weekday = (Weekday)tm->tm_wday;
		date.dst = tm->tm_isdst > 0;
	}
	return date;
}

OS::Time OS_Xbox360Xsdk::get_time(bool) const {
	Time time = {};
	time_t t = ::time(nullptr);
	struct tm buf = {};
	struct tm *tm = localtime_r(&t, &buf);
	if (tm) {
		time.hour = tm->tm_hour;
		time.min = tm->tm_min;
		time.sec = tm->tm_sec;
	}
	return time;
}

OS::TimeZoneInfo OS_Xbox360Xsdk::get_time_zone_info() const {
	TimeZoneInfo info = {};
	info.bias = 0;
	info.name = "UTC";
	return info;
}

void OS_Xbox360Xsdk::delay_usec(uint32_t p_usec) const {
	const uint64_t start = _monotonic_usec();
	const uint64_t target = start + p_usec;
	while (_monotonic_usec() < target) {
		// Busy-wait. xsdk's KeDelayExecutionThread would put the thread to sleep,
		// but keeping the call simple avoids surprising the main loop.
	}
}

uint64_t OS_Xbox360Xsdk::get_ticks_usec() const {
	return _monotonic_usec() - timebase_start;
}

uint64_t OS_Xbox360Xsdk::get_unix_time() const {
	return (uint64_t)::time(nullptr);
}

void OS_Xbox360Xsdk::run() {
	if (!main_loop) {
		xsdk_log_write("xbox360xsdk: no main loop, aborting run");
		return;
	}

	main_loop->init();
	xsdk_log_write("xbox360xsdk: entering main loop");

	while (true) {
		if (input) {
			INPUT_STATE state = {};
			if (XamInputGetState(0, 0, &state) == ERROR_SUCCESS) {
				const uint16_t buttons = state.gamepad.buttons;
				const uint8_t lt = state.gamepad.left_trigger;
				const uint8_t rt = state.gamepad.right_trigger;

				input->joy_button(0, JOY_XBOX_A, (buttons & INPUT_GAMEPAD_A) != 0);
				input->joy_button(0, JOY_XBOX_B, (buttons & INPUT_GAMEPAD_B) != 0);
				input->joy_button(0, JOY_XBOX_X, (buttons & INPUT_GAMEPAD_X) != 0);
				input->joy_button(0, JOY_XBOX_Y, (buttons & INPUT_GAMEPAD_Y) != 0);
				input->joy_button(0, JOY_L, (buttons & INPUT_GAMEPAD_LEFT_SHOULDER) != 0);
				input->joy_button(0, JOY_R, (buttons & INPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
				input->joy_button(0, JOY_L2, lt > 128);
				input->joy_button(0, JOY_R2, rt > 128);
				input->joy_button(0, JOY_L3, (buttons & INPUT_GAMEPAD_LEFT_THUMB) != 0);
				input->joy_button(0, JOY_R3, (buttons & INPUT_GAMEPAD_RIGHT_THUMB) != 0);
				input->joy_button(0, JOY_SELECT, (buttons & INPUT_GAMEPAD_BACK) != 0);
				input->joy_button(0, JOY_START, (buttons & INPUT_GAMEPAD_START) != 0);
				input->joy_button(0, JOY_GUIDE, (buttons & INPUT_GAMEPAD_GUIDE) != 0);

				input->joy_button(0, JOY_DPAD_UP, (buttons & INPUT_GAMEPAD_DPAD_UP) != 0);
				input->joy_button(0, JOY_DPAD_DOWN, (buttons & INPUT_GAMEPAD_DPAD_DOWN) != 0);
				input->joy_button(0, JOY_DPAD_LEFT, (buttons & INPUT_GAMEPAD_DPAD_LEFT) != 0);
				input->joy_button(0, JOY_DPAD_RIGHT, (buttons & INPUT_GAMEPAD_DPAD_RIGHT) != 0);

				int hat = 0;
				if (buttons & INPUT_GAMEPAD_DPAD_UP) hat |= 1;
				if (buttons & INPUT_GAMEPAD_DPAD_RIGHT) hat |= 2;
				if (buttons & INPUT_GAMEPAD_DPAD_DOWN) hat |= 4;
				if (buttons & INPUT_GAMEPAD_DPAD_LEFT) hat |= 8;
				input->joy_hat(0, hat);

				input->joy_axis(0, JOY_ANALOG_LX, state.gamepad.thumb_lx / 32768.0f);
				input->joy_axis(0, JOY_ANALOG_LY, state.gamepad.thumb_ly / 32768.0f);
				input->joy_axis(0, JOY_ANALOG_RX, state.gamepad.thumb_rx / 32768.0f);
				input->joy_axis(0, JOY_ANALOG_RY, state.gamepad.thumb_ry / 32768.0f);
				input->joy_axis(0, JOY_ANALOG_L2, lt / 255.0f);
				input->joy_axis(0, JOY_ANALOG_R2, rt / 255.0f);
			}

			// Godot 3.x accumulates input events by default. Other platforms
			// flush them at the end of their event polling; without this the
			// joypad events above never reach the scene tree.
			input->flush_buffered_events();
		}

		if (Main::iteration()) {
			break;
		}
	}

	xsdk_log_write("xbox360xsdk: main loop finished");
	main_loop->finish();
}
