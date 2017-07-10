
#define SD_JOURNAL_SUPPRESS_LOCATION

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/uio.h>
#include <systemd/sd-journal.h>
#include <systemd/sd-daemon.h>
#else
#include <iostream>
#endif

#include <iostream>
#include <algorithm>
#include "beautylog/beautylog.hpp"

static bool conhost_is_vt_mode = false;

static void send_structured_message_lines_journald(const std::vector<std::string>& vector_field_string, const std::string message);
static void send_structured_message_lines_win_conhost_vtmode(const std::vector<std::string>& vector_field_string, const std::string message);
static void send_structured_message_lines_win_conhost(const std::vector<std::string>& vector_field_string, const std::string message);
static void send_structured_message_lines_stdout(const std::vector<std::string>& vector_field_string, const std::string message);

namespace beautylog
{
	send_structured_message_lines_fn_t send_structured_message_lines_impl = nullptr;

	sd_loger::sd_loger()
	{
	}

	sd_loger::~sd_loger()
	{
		flush();
	}

	void sd_loger::flush()
	{
		std::unique_lock<std::mutex> l(m_mutex);
		auto log_entries_copyed = std::move(log_entries);

		l.unlock();

		for (auto e : log_entries_copyed)
		{
			send_structured_message(e);
		}
		log_entries.clear();
	}

	void sd_loger::disable_defer()
	{
		no_defer = true;
		flush();
	}

	void sd_loger::enable_defer()
	{
		no_defer = false;
	}


	void sd_loger::discardlog(int priority)
	{
		std::unique_lock<std::mutex> l(m_mutex);

		for (std::vector<log_entry>::iterator it = log_entries.begin(); it != log_entries.end();)
		{
			if (it->priority >= priority)
			{
				it = log_entries.erase(it);
			}
			else
			{
				it++;
			}
		}
	}

	void sd_loger::add_constant_field(std::string field_name, std::string field_value)
	{
		std::unique_lock<std::mutex> l(m_mutex);
		constant_fields.insert({ field_name, field_value });
	}

	void sd_loger::set_constant_fields(const std::map<std::string, std::string>& cf)
	{
		std::unique_lock<std::mutex> l(m_mutex);
		constant_fields = cf;
	}

	void sd_loger::send_structured_message(const log_entry&e) const
	{
		std::unique_lock<std::mutex> l(m_mutex);

		std::vector<std::string> vector_string;

		vector_string.push_back("PRIORITY=" + std::to_string(e.priority));
		for (const auto& f : e.log_fields)
		{
			vector_string.push_back(f.first + "=" + f.second);
		}

		for (const auto& f : constant_fields)
		{
			vector_string.push_back(f.first + "=" + f.second);
		}

		send_structured_message_lines_impl(vector_string, e.log_message);
	}

	std::string sd_loger::cpp_file_pretty(const char * file)
	{
#ifdef _WIN32
		auto p = strstr(file, "flashpay\\");
#else
		auto p = strstr(file, "flashpay/");
#endif
		if (p)
		{
			return p + 9;
		}
		return file;
	}
}

#ifdef _WIN64

class AutoCalledCode
{
public:
	AutoCalledCode()
	{
		// Set output mode to handle virtual terminal sequences
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE)
		{
			conhost_is_vt_mode = false;
			beautylog::send_structured_message_lines_impl = send_structured_message_lines_stdout;
			return;
		}

		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode))
		{
			conhost_is_vt_mode = false;
			beautylog::send_structured_message_lines_impl = send_structured_message_lines_win_conhost;
			return;
		}

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

		if (!SetConsoleMode(hOut, dwMode))
		{
			conhost_is_vt_mode = false;
			beautylog::send_structured_message_lines_impl = send_structured_message_lines_win_conhost;
			return;
		}
		else
		{
			conhost_is_vt_mode = true;
		}

		beautylog::send_structured_message_lines_impl = send_structured_message_lines_win_conhost_vtmode;
	}
};

#elif defined(__linux__)

class AutoCalledCode
{
public:
	AutoCalledCode()
	{
		if (sd_booted())
			beautylog::send_structured_message_lines_impl = send_structured_message_lines_journald;
		else
			beautylog::send_structured_message_lines_impl = send_structured_message_lines_stdout;
	}
};

#else

class AutoCalledCode
{
public:
	AutoCalledCode()
	{
		beautylog::send_structured_message_lines_impl = send_structured_message_lines_stdout;
	}
};

#endif

static AutoCalledCode auto_code;

#ifdef _WIN32
static std::u16string ConvertUTF8ToUTF16(std::string u8str)
{
	std::size_t wchar_length = ::MultiByteToWideChar(
		CP_UTF8,                // convert from UTF-8
		0,   // error on invalid chars
		u8str.c_str(),      // source UTF-8 string
		(int)u8str.length() + 1, // total length of source UTF-8 string,
								 // in CHAR's (= bytes), including end-of-string \0
		NULL,               // unused - no conversion done in this step
		0                   // request size of destination buffer, in WCHAR's
	);

	if (wchar_length == 0)
	{
		return u"";
	}

	std::u16string wstr;

	wstr.resize(wchar_length);

	//
	// Do the conversion from UTF-8 to UTF-16
	//
	int result = ::MultiByteToWideChar(
		CP_UTF8,                // convert from UTF-8
		0,   // error on invalid chars
		u8str.c_str(),      // source UTF-8 string
		(int)u8str.length() + 1, // total length of source UTF-8 string,

		(wchar_t*)&wstr[0],               // destination buffer
		(int)wchar_length            // size of destination buffer, in WCHAR's
	);


	// Return resulting UTF16 string
	return wstr;
}

void send_structured_message_lines_win_conhost(const std::vector<std::string>& vector_field_string, const std::string message)
{
	{
		std::string msgline;

		for (auto & s : vector_field_string)
		{
			msgline += "[" + s + "]";
		}
		std::u16string l = ConvertUTF8ToUTF16(msgline);
		OutputDebugStringW((const wchar_t*)l.c_str());
	}

	HANDLE handle_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(handle_stdout, &csbi);

	std::vector<DWORD> color_cyc_buffer = {
		FOREGROUND_GREEN,
		FOREGROUND_INTENSITY | FOREGROUND_GREEN,
		FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY,
		FOREGROUND_RED | FOREGROUND_INTENSITY,
		FOREGROUND_RED | FOREGROUND_BLUE,
		FOREGROUND_BLUE | FOREGROUND_INTENSITY
	};

	for (int i = 0; i < vector_field_string.size(); i++)
	{
		auto s = "[" + vector_field_string[i] + "]";

		std::u16string l = ConvertUTF8ToUTF16(s);
		SetConsoleTextAttribute(handle_stdout, color_cyc_buffer[i % color_cyc_buffer.size()]);
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), l.data(), l.length(), nullptr, nullptr);
	}

	SetConsoleTextAttribute(handle_stdout, csbi.wAttributes);

	std::u16string l = ConvertUTF8ToUTF16(message) + u"\r\n";

	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), l.data(), l.length(), nullptr, nullptr);
}

void send_structured_message_lines_win_conhost_vtmode(const std::vector<std::string>& vector_field_string, const std::string message)
{
	{
		std::string msgline;

		for (auto & s : vector_field_string)
		{
			msgline += "[" + s + "]";
		}
		std::u16string l = ConvertUTF8ToUTF16(msgline);
		OutputDebugStringW((const wchar_t*)l.c_str());
	}

	auto vt_escape = [](int bold, int red, int green, int blue) -> std::string
	{
		char buf[100] = { 0 };
		if (!bold)
			sprintf_s(buf, sizeof buf, "\x1b[38;2;%d;%d;%dm", red, green, blue);
		else
			sprintf_s(buf, sizeof buf, "\x1b[1m\x1b[38;2;%d;%d;%dm", red, green, blue);
		return buf;
	};

	struct rgbmode
	{
		int red, green, blue, bold;
	};

	std::vector<rgbmode> color_cyc_buffer = {
		{ 255, 0 , 0 },
		{ 255, 127, 0 },
		{ 255, 255, 0 },
		{ 0, 255, 0 },
		{ 0, 0, 255 },
		{ 75, 0, 130 },
		{ 148, 0, 211 },
	};

	for (int i = 0; i < vector_field_string.size(); i++)
	{
		//printf("\x1b[48;2;%d;%d;%dm", red, green, blue); // produces RGB background
		rgbmode clr = color_cyc_buffer[i % color_cyc_buffer.size()];

		auto s = vt_escape(clr.bold, clr.red, clr.green, clr.blue) + "[" + vector_field_string[i] + "]";

		std::u16string l = ConvertUTF8ToUTF16(s);
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), l.data(), l.length(), nullptr, nullptr);
	}

	auto coloredmessage = "\x1b[1m\x1b[0m" + message;

	std::u16string l = ConvertUTF8ToUTF16(coloredmessage) + u"\r\n";
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), l.data(), l.length(), nullptr, nullptr);
}

#endif

void send_structured_message_lines_stdout(const std::vector<std::string>& vector_field_string, const std::string message)
{
	std::cout << message << std::endl;
}

#ifdef __linux__
void send_structured_message_lines_journald(const std::vector<std::string>& vector_field_string, const std::string message)
{
	std::vector<iovec> iovecs(vector_field_string.size() + 1);

	for (int i = 0; i < vector_field_string.size(); i++)
	{
		iovecs[i].iov_base = const_cast<char*>(vector_field_string[i].data());
		iovecs[i].iov_len = vector_field_string[i].size();
	}

	std::string msg_field = "MESSAGE=" + message;

	iovecs[vector_field_string.size()].iov_base = const_cast<char*>(msg_field.c_str());
	iovecs[vector_field_string.size()].iov_len = msg_field.size();

	sd_journal_sendv(&iovecs[0], iovecs.size());
}
#endif