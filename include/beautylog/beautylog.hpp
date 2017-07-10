
#pragma once

#include <mutex>
#include <vector>
#include <map>
#include <memory>

#include "boost/date_time/posix_time/posix_time.hpp"


#include "fmt_helper.hpp"

namespace beautylog
{
	typedef void(*send_structured_message_lines_fn_t)(const std::vector<std::string>& vector_field_string, const std::string message);
	extern send_structured_message_lines_fn_t send_structured_message_lines_impl;

	struct log_entry
	{
		int priority = 0;
		boost::posix_time::ptime msg_time;
		std::string log_message;
		std::map<std::string, std::string> log_fields;
	};

	class sd_loger
	{
	public:
		sd_loger();
		~sd_loger();

		static std::string cpp_file_pretty(const char* file);

		template<typename... Args>
		void log_with_location(int priority, const char* file, int sourceline, const char* func, const std::string& fmt, Args&&... args)
		{
			log_entry this_entry;

			this_entry.log_fields["CODE_FILE"] = cpp_file_pretty(file);
			this_entry.log_fields["CODE_FUNC"] = func;
			this_entry.log_fields["CODE_LINE"] = std::to_string(sourceline);

			this_entry.msg_time = boost::posix_time::microsec_clock::local_time();

			this_entry.priority = priority;

			this_entry.log_message = format_msg(fmt, std::forward<Args>(args)...);

			if (no_defer)
			{
				send_structured_message(this_entry);
			}
			else
			{
				std::unique_lock<std::mutex> l(m_mutex);
				log_entries.push_back(this_entry);
			}
		}

		void send_structured_message(const log_entry&e) const;

	public:
		void flush();
		void disable_defer();
		void enable_defer();

		// discard log that has priority<priority
		void discardlog(int priority = 3);

		void add_constant_field(std::string field_name, std::string field_value);
		void set_constant_fields(const std::map<std::string, std::string>& cf);

	private:
		std::map<std::string, std::string> constant_fields;
		std::vector<log_entry> log_entries;
		bool no_defer = false;
		mutable std::mutex m_mutex;
	};

	template<typename... Args>
	void log_with_location_but_no_constant_field(int priority, const char* file, int sourceline, const char* func, const std::string& fmt, Args&&... args)
	{
		log_entry this_entry;

		this_entry.log_fields["CODE_FILE"] = sd_loger::cpp_file_pretty(file);
		this_entry.log_fields["CODE_FUNC"] = func;
		this_entry.log_fields["CODE_LINE"] = std::to_string(sourceline);

		this_entry.msg_time = boost::posix_time::microsec_clock::local_time();

		this_entry.priority = priority;

		this_entry.log_message = format_msg(fmt, std::forward<Args>(args)...);

		std::vector<std::string> vector_string;

		vector_string.push_back("PRIORITY=" + std::to_string(this_entry.priority));
		for (const auto& f : this_entry.log_fields)
		{
			vector_string.push_back(f.first + "=" + f.second);
		}

		send_structured_message_lines_impl(vector_string, this_entry.log_message);
	}
} // namespace beautylog

#ifdef _MSC_VER
#define beautylog_print(loger, priority, fmt, ...) loger->log_with_location(priority, __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#define log_print(priority, fmt, ...) beautylog::log_with_location_but_no_constant_field(priority, __FILE__, __LINE__, __func__, fmt, __VA_ARGS__)
#else
#define beautylog_print(loger, priority,  ...) loger->log_with_location(priority, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_print(priority,  ...) beautylog::log_with_location_but_no_constant_field(priority, __FILE__, __LINE__, __func__, __VA_ARGS__)
#endif

typedef std::shared_ptr<beautylog::sd_loger> sdloger_t;

#ifndef LOG_EMERG
	#define	LOG_EMERG	0	/* system is unusable */
	#define	LOG_ALERT	1	/* action must be taken immediately */
	#define	LOG_CRIT	2	/* critical conditions */
	#define	LOG_ERR		3	/* error conditions */
	#define	LOG_WARNING	4	/* warning conditions */
	#define	LOG_NOTICE	5	/* normal but significant condition */
	#define	LOG_INFO	6	/* informational */
	#define	LOG_DEBUG	7	/* debug-level messages */
#endif
