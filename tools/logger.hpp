/** 
 @cond
 #########################################################################
 # GPL License                                                           #
 #                                                                       #
 # This file is part of the Machine Learning Framework.                  #
 # Copyright (c) 2010, Philipp Kraus, <philipp.kraus@flashpixx.de>       #
 # This program is free software: you can redistribute it and/or modify  #
 # it under the terms of the GNU General Public License as published by  #
 # the Free Software Foundation, either version 3 of the License, or     #
 # (at your option) any later version.                                   #
 #                                                                       #
 # This program is distributed in the hope that it will be useful,       #
 # but WITHOUT ANY WARRANTY; without even the implied warranty of        #
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
 # GNU General Public License for more details.                          #
 #                                                                       #
 # You should have received a copy of the GNU General Public License     #
 # along with this program.  If not, see <http://www.gnu.org/licenses/>. #
 #########################################################################
 @endcond
 **/


#ifndef MACHINELEARNING_TOOLS_LOGGER_HPP
#define MACHINELEARNING_TOOLS_LOGGER_HPP

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>

#include <boost/thread.hpp>

#ifdef CLUSTER
#define LOGGER_MPI_TAG 999
#include <boost/mpi.hpp>
#include <boost/bind.hpp>
#endif

#include "../exception/exception.h"
#include "language/language.h"


namespace machinelearning { namespace tools { 
    
    #ifdef CLUSTER
    namespace mpi   = boost::mpi;
    #endif
    

    /** logger class for writing log information 
     * @note for MPI using every process must call startListener and shutdownListener for synchronize the CPUs and the tag for logtargets ist set with a preprocessor flag (LOGGER_MPI_TAG).
     * The singletone object works only "between" the calls start- and shutdownListener because the MPI object must exists.
     * @todo tmpnam(NULL) change to a safe variante (posix mkstemp(), but this isn't standard)
     **/
    class logger {
        
        public :
        
            enum logstate {
                none  = 0,
                error = 1,
                warn  = 2,
                info  = 3
            };
                
            
            static logger* getInstance( void );
            void setLevel( const logstate& );
            logstate getLevel( void ) const;
            std::string getFilename( void ) const;
            template<typename T> void write( const logstate&, const T& );
                            
            #ifdef CLUSTER
            void startListener( const mpi::communicator& );
            void shutdownListener( const mpi::communicator& );
            template<typename T> void write( const mpi::communicator&, const logstate&, const T& );
            #endif
        
        
        
        private : 
        
            /** local instance **/
            static logger* m_instance;
            /** filename for logging output **/
            static std::string m_filename;
            /** logstate for writing data **/
            logstate m_logstate;
            /** file handle **/
            std::ofstream m_file;
            /** mutex for locking **/
            boost::mutex m_muxwriter;
        
        
            logger( void );  
            ~logger( void ); 
            logger( const logger& );
            logger& operator=( const logger& );
        
            template<typename T> void logformat( const logstate&, const T&, std::ostringstream& ) const;
            void write2file( const std::ostringstream& );
        
        
            #ifdef CLUSTER
        
            /** mutex for creating the listener **/
            boost::mutex m_muxlistener;
            /** mutex for finalizing **/
            boost::mutex m_muxfinalize;
            /** bool for running the listener **/
            bool m_listenerrunnging;
        
            void listener( const mpi::communicator& );
        
            #endif
        
    };
    
    
    /** constructor **/
    inline logger::logger( void ) :
        m_logstate(none),
        m_muxwriter()
        #ifdef CLUSTER
        , m_muxlistener(),
        m_muxfinalize(),
        m_listenerrunnging(false)
        #endif
    {};
    
    
    /** copy constructor **/
    inline logger::logger( const logger& ) {}

    
    /** destructor **/
    inline logger::~logger( void )
    {
        #ifdef CLUSTER
        m_listenerrunnging = false;
        #endif
        m_file.close();
    }
    

    /** equal operator **/
    inline logger& logger::operator=( const logger& )
    {
        return *this;
    }

    
    /** returns the instance
     * @return logger instance
     **/
    inline logger* logger::getInstance()
    {
        if (!m_instance)
            m_instance = new logger();

        return m_instance;
    }
    
    
    /** returns the temporary filename for logging
     * @return string with path and filename
     **/
    inline std::string logger::getFilename( void ) const
    {
        return m_filename;
    }
    
    
    /** set the log state
     * @paramp p_state state for logging
     **/
    inline void logger::setLevel( const logstate& p_state )
    {
        m_logstate = p_state;
    }
    
    
    /** returns the status of logging
     * @return boolean status
     **/
    inline logger::logstate logger::getLevel( void ) const
    {
        return m_logstate;
    }
    
    
    /** writes the data in the local log file 
     * @param p_state log level
     * @param p_val value
     **/
    template<typename T> inline void logger::write( const logstate& p_state, const T& p_val ) {
        if ( (m_logstate == none) || (p_state == none) || (p_state > m_logstate) )
            return;
  
        std::ostringstream l_stream;
        l_stream << "local - ";
        logformat(p_state, p_val, l_stream);
        write2file( l_stream );
    }
    
    
    /** create the log format 
     * @param p_state log level
     * @param p_val value for writing
     * @param p_stream reference for input the data
     **/
    template<typename T> inline void logger::logformat( const logstate& p_state, const T& p_val, std::ostringstream& p_stream ) const
    {
        switch (p_state) {
            case info   : p_stream << "[info]       " << p_val;   break;
            case warn   : p_stream << "[warn]       " << p_val;   break;
            case error  : p_stream << "[error]      " << p_val;   break;
                
            default     : throw exception::runtime(_("log state is unkown"));
        }
    }
    
    
    /** writes the output stream to the file with thread locking
     * @param p_data output stream
     **/
    inline void logger::write2file( const std::ostringstream& p_data )
    {
        if (p_data.str().empty())
            return;
        
        // lock will remove with the destructor call
        boost::lock_guard<boost::mutex> l_lock(m_muxwriter);         
        
        if (!m_file.is_open())
            m_file.open( m_filename.c_str(), std::ios_base::app );
        
        m_file << p_data.str() << "\n";
        m_file.flush();
    }

    
    
    //======= MPI ==================================================================================================================================
    #ifdef CLUSTER
    
    /** creates the local listener on CPU 0
      * @param p_mpi MPI object
     **/
    inline void logger::startListener( const mpi::communicator& p_mpi )
    {
       p_mpi.barrier();
       if ( (p_mpi.rank() != 0) || (p_mpi.size() == 1) || m_listenerrunnging )
            return;
        
       // lock will remove with the destructor call
       boost::lock_guard<boost::mutex> l_lock(m_muxlistener); 

       m_listenerrunnging = true;
       boost::thread l_thread( boost::bind( &logger::listener, this, boost::cref(p_mpi)) );
    }
    
    
    /** shutdown the listener thread and synchronize the CPUs
     * @param p_mpi MPI object
     **/
    inline void logger::shutdownListener( const mpi::communicator& p_mpi ) {
        m_listenerrunnging = false;
        p_mpi.barrier();
        
        // for the CPU 0 we wait (if needed) that the thread function is finalized
        if (p_mpi.rank() == 0)
            boost::lock_guard<boost::mutex> l_lock(m_muxfinalize);
    }
    
    
    /** write log entry. If the CPU rank == 0 the log will write to the file, on other CPU rank the message
     * is send to the CPU 0 and write down there. The local log state is relevant for writing
     * @param p_mpi MPI object
     * @param p_state log level
     * @param p_val value
     **/     
    template<typename T> inline void logger::write( const mpi::communicator& p_mpi, const logstate& p_state, const T& p_val )
    {
        if ( (m_logstate == none) || (p_state == none) || (p_state > m_logstate) )
            return;
        
        std::ostringstream l_stream;
        l_stream << "CPU " << p_mpi.rank() << " - ";
        logformat(p_state, p_val, l_stream);
        
        if (p_mpi.rank() == 0)
            write2file( l_stream );
        else
            p_mpi.isend(0, LOGGER_MPI_TAG, l_stream.str());
    }
    
    
    /** thread method that receive the asynchrone messages of the MPI interface.
     * The listener method read the message and writes them down
     * @param p_mpi MPI object
     **/
    inline void logger::listener( const mpi::communicator& p_mpi )
    {
        boost::lock_guard<boost::mutex> l_lock(m_muxfinalize);
        
        while (m_listenerrunnging) {
            
            while (boost::optional<mpi::status> l_status = p_mpi.iprobe(mpi::any_source, LOGGER_MPI_TAG)) {
                std::string l_str;
                std::ostringstream l_stream;
                    
                p_mpi.recv( l_status->source(), l_status->tag(), l_str);
                l_stream << l_str;
                write2file( l_stream );
            }
            
            boost::this_thread::yield();
        }
    }

    
    #endif
    
    
};};



#endif