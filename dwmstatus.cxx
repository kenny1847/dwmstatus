#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>

#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <X11/Xlib.h>

#ifdef USE_MPD
#	include <mpd/client.h>
#endif

class string_builder {
	std::string _result;

public:
	string_builder& operator % (const std::string& str) {
		_result.append(str);
		return *this;
	}
	string_builder& operator % (const char* str) {
		_result.append(str);
		return *this;
	}
	template <typename T>
	string_builder& operator % (T val) {
		_result.append(std::to_string(val));
		return *this;
	}

	operator std::string () const {
		return _result;
	}
};

std::string get_date_time(const char* fmt) {
	char buf[30] = {0};
	const std::time_t tim = std::time(nullptr);
	if (const std::tm* timtm = std::localtime(&tim)) {
		if (std::strftime(buf, sizeof(buf), fmt, timtm)) {
			return buf;
		}
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}
	perror("localtime");
	exit(1);
}

template <typename T>
bool read_value(T& val, const std::string& path) {
	std::ifstream fin(path);
	fin >> val;
	return !fin.fail();
}

std::string get_battery() {
	int capacity;
	std::string status;
	if (read_value(capacity, "/sys/class/power_supply/BAT0/capacity")) {
		if (read_value(status, "/sys/class/power_supply/BAT0/status")) {
			string_builder b;
			if (status == "Charging") { b % " "; }
			else {
				if (capacity >= 80)                       { b % " "; }
				else if (capacity >= 60 && capacity < 80) { b % " "; }
				else if (capacity >= 40 && capacity < 60) { b % " "; }
				else if (capacity >= 20 && capacity < 40) { b % " "; }
				else                                      { b % " "; }
			}
			return b % capacity % "%";
		}
	}
	return std::string();
}

long get_vol() {
	snd_hctl_t* hctl;
	/* To find card and subdevice: /proc/asound/, aplay -L, amixer controls */
	snd_hctl_open(&hctl, "hw:1", 1);
	snd_hctl_load(hctl);

	snd_ctl_elem_id_t* id;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

	/* amixer controls */
	snd_ctl_elem_id_set_name(id, "Master Playback Volume");

	snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);

	snd_ctl_elem_value_t* control;
	snd_ctl_elem_value_alloca(&control);
	snd_ctl_elem_value_set_id(control, id);

	snd_hctl_elem_read(elem, control);
	const long vol = snd_ctl_elem_value_get_integer(control, 0);

	snd_hctl_close(hctl);
	return (vol*100/87);
}

std::string get_mpd_current_song() {
	std::string result;
#ifdef USE_MPD
	mpd_connection* conn = mpd_connection_new("localhost", 0, 30000);
	if (!conn || mpd_connection_get_error(conn)) {
		return result;
	}

	mpd_command_list_begin(conn, true);
	mpd_send_status(conn);
	mpd_send_current_song(conn);
	mpd_command_list_end(conn);

	if (mpd_status* theStatus = mpd_recv_status(conn)) {
		if (mpd_status_get_state(theStatus) == MPD_STATE_PLAY) {
			mpd_response_next(conn);
			if (mpd_song* song = mpd_recv_song(conn)) {
				const int elapsed = mpd_status_get_elapsed_time(theStatus);
				const int total = mpd_status_get_total_time(theStatus);

				result = string_builder()
						% "  " % (elapsed/60) % ":" % (elapsed%60)
						% "/" % (total/60) % ":" % (total%60)
						% " " % mpd_song_get_tag(song, MPD_TAG_ARTIST, 0) % " - " % mpd_song_get_tag(song, MPD_TAG_TITLE, 0);

				mpd_song_free(song);
			}
		}
		mpd_status_free(theStatus);
	}
	mpd_response_finish(conn);
	mpd_connection_free(conn);
#endif
	return result;
}

int main(int /* argc */, char** /* argv */) {
	if (Display* dpy = XOpenDisplay(nullptr)) {
		for (;;usleep(500000)) {
			std::string result = string_builder()
					% get_mpd_current_song() % "   " % get_vol() % "%  "
					% get_battery() % get_date_time("   %a %d.%m   %H:%M");
			XStoreName(dpy, DefaultRootWindow(dpy), result.c_str());
			XSync(dpy, False);
		}

		XCloseDisplay(dpy);
		return 0;
	}
	fprintf(stderr, "Cannot open display.\n");
	return 1;
}
