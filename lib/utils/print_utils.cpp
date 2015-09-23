#include "utils/print_utils.h"
#include "MG_config.h"

#include <string>
#include <cstdlib>
#include <cstdarg>

#ifdef QMP_COMMS
#include "qmp.h"
#endif

namespace MGUtils {

	/* Current Log Level */
	static LogLevel current_log_level = MG_DEFAULT_LOGLEVEL;

	/* An array holding strings corresponding to log levels */
	static std::string log_string_array[] = {"ERROR", "INFO", "DEBUG", "DEBUG2", "DEBUG3"};


	/**
	 * SetLogLevel -- set the log level
	 *
	 * \param level  -- The LogLevel to set. LogMessages with levels <= level will be printed
	 *
	 * NB: This function may be called in several MPI processes, in which case it needs to
	 * be called collectively. Being called in one MPI process and not in another is considered
	 * a programming error. Likewise the function may be concurrently called from several
	 * OpenMP threads. If called from many threads potentially. While it would be weird for
	 * all threads to set different log levels, the safe thing to do is to guard the write with
	 * an OMP Criticla section
	 *
	 */
	void SetLogLevel(LogLevel level)
	{
#pragma omp critical
		{
			current_log_level = level;
		}
	}

	/**
	 * SetLogLevel - get the log level
	 *
	 * \returns  The current log level
	 *
	 * NB: The design is for the loglevel to be kept on each MPI process. This function only
	 * reads the loglevel value, so no races can occur.
	 */
	LogLevel GetLogLevel(void) {
		return current_log_level;
	}


	/**
	 * 	LocalLog - Local process performs logging
	 * 	\param level -- if the level is <= the current log level, a message will be printed
	 * 	\param format_string
	 * 	\param variable list of arguments
	 *
	 * 	Current definition is that only the master thread on each nodes logs
	 */
	void LocalLog(LogLevel level, const char*  format, ...)
	{

#pragma omp master
		{
			if( level <= current_log_level ) {
#ifdef QMP_COMMS
				int size = QMP_get_number_of_nodes();
				int rank = QMP_get_node_number();
#else
				int size = 1;
				int rank = 0;
#endif
				printf("%s: Rank %d of %d: ", log_string_array[level].c_str(), rank, size);
				va_list args;
				va_start(args,format);
				vprintf(format, args);
				va_end(args);
			}	/* end If */

			/* If the level is error than we should abort */
			if( level == ERROR ) {
#ifdef QMP_COMMS
				QMP_abort(1);
#else
				std::abort();
#endif

			} /* if level == ERROR */

		} /* End of OMP_MASTER_REGION */
	}


	/**
	 * 	MasterlLog - Master  process performs logging
	 * 	\param level -- if the level is <= the current log level, a message will be printed
	 * 	\param format_string
	 * 	\param variable list of arguments
	 *
	 * 	Current definition is that only the master thread on each nodes logs
	 */
    void MasterLog(LogLevel level, const char *format, ...)
    {
#ifdef QMP_COMMS
    		if ( QMP_is_primary_node() )  {
#endif
#pragma omp master
    			{
    				if( level <= current_log_level ) {

    					printf("%s: ", log_string_array[level].c_str());
    					va_list args;
    					va_start(args,format);
    					vprintf(format, args);
    					va_end(args);
    				}	/* end If */

    				/* If the level is error than we should abort */
    				if( level == ERROR ) {
#ifdef QMP_COMMS
    					QMP_abort(1);
#else
    					std::abort();
#endif
    				} /* if level == ERROR */

    			} /* End of OMP_MASTER_REGION */
#ifdef QMP_COMMS
    		} /* if ( QMP_is_primary_node())  */
#endif
    }


} /* End Namespace MGUtils */
