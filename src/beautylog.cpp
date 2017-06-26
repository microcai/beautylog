
#define SD_JOURNAL_SUPPRESS_LOCATION

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/uio.h>
#include <systemd/sd-journal.h>
#endif

#include <iostream>
#include <algorithm>
#include "beautylog/beautylog.hpp"

namespace beautylog
{
	sd_loger::sd_loger()
	{
	}

	sd_loger::~sd_loger()
	{
		flush();
	}

#ifdef _WIN32
	static std::wstring ConvertUTF8ToUTF16(std::string u8str)
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
			return L"";
		}

		std::wstring wstr;

		wstr.resize(wchar_length);

		//
		// Do the conversion from UTF-8 to UTF-16
		//
		int result = ::MultiByteToWideChar(
			CP_UTF8,                // convert from UTF-8
			0,   // error on invalid chars
			u8str.c_str(),      // source UTF-8 string
			(int)u8str.length() + 1, // total length of source UTF-8 string,

			&wstr[0],               // destination buffer
			(int)wchar_length            // size of destination buffer, in WCHAR's
		);


		// Return resulting UTF16 string
		return wstr;
	}


	void sd_loger::send_send_structured_message_lines(const std::vector<std::string>& msg_lines)
	{
		std::string msgline;

		for (auto & s : msg_lines)
		{
			msgline += "[" + s + "]";
		}

		std::wstring l = ConvertUTF8ToUTF16(msgline);

		OutputDebugStringW(l.c_str());
		std::wcerr << l << std::endl;
	}
#else
	void sd_loger::send_send_structured_message_lines(const std::vector<std::string>& vector_string)
	{
		std::vector<iovec> iovecs(vector_string.size());

		for (int i = 0; i < vector_string.size(); i++)
		{
			iovecs[i].iov_base = const_cast<char*>(vector_string[i].data());
			iovecs[i].iov_len = vector_string[i].size();
		}

		sd_journal_sendv(&iovecs[0], iovecs.size());
	}
#endif

	void sd_loger::flush()
	{
		std::unique_lock<std::mutex> l(m_mutex);
		auto log_entries_copyed = std::move(log_entries);

		l.unlock();

		for (auto e : log_entries)
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
			if (it->priority <= priority)
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

		vector_string.push_back("MESSAGE=" + e.log_message);
		send_send_structured_message_lines(vector_string);
	}
}
