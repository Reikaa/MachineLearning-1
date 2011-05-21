/** 
 @cond
 ############################################################################
 # LGPL License                                                             #
 #                                                                          #
 # This file is part of the Machine Learning Framework.                     #
 # Copyright (c) 2010, Philipp Kraus, <philipp.kraus@flashpixx.de>          #
 # This program is free software: you can redistribute it and/or modify     #
 # it under the terms of the GNU Lesser General Public License as           #
 # published by the Free Software Foundation, either version 3 of the       #
 # License, or (at your option) any later version.                          #
 #                                                                          #
 # This program is distributed in the hope that it will be useful,          #
 # but WITHOUT ANY WARRANTY; without even the implied warranty of           #
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            #
 # GNU Lesser General Public License for more details.                      #
 #                                                                          #
 # You should have received a copy of the GNU Lesser General Public License #
 # along with this program. If not, see <http://www.gnu.org/licenses/>.     #
 ############################################################################
 @endcond
 **/



#ifndef MACHINELEARNING_TEXTPROCESS_HISTOGRAM_HPP
#define MACHINELEARNING_TEXTPROCESS_HISTOGRAM_HPP

#include <map>
#include <string>
#include <boost/algorithm/string.hpp> 

#include "../exception/exception.h"



namespace machinelearning { namespace textprocess {

    
    /** class for text histogram. The class analyses texts and create a map with word counts,
     * that can be used for stop-word-reduction
     * $LastChangedDate$
     * @todo switch to unicode
     **/
    class histogram {
        
        public:
        
            enum comparison {
                lessequal       = 0,
                greaterequal    = 1,
                less            = 2,
                greater         = 3
            };
        
        
            histogram( const std::string& = ",;.:!?- ", const bool& = false );
            void add( const std::string&, const std::size_t& = 0 );
            void add( const std::vector<std::string>&, const std::size_t& = 0 );
            bool getCaseSensitivity( void ) const;
            std::size_t getWordCount( void ) const;
            std::vector<std::string> getWords( const float&, const float&, const comparison& = lessequal, const comparison& = greaterequal );
            std::vector<std::string> getWords( const float&, const comparison& = lessequal );
        
        
        private:
            
            /** seperators **/
            const std::string m_seperators;        
            /** bool for case-sensitive / case-insensitive wordlist **/
            const bool m_casesensitive;
            /** map with words and their counts **/
            std::map<std::string, std::size_t> m_map;
            /** sum over all words **/
            std::size_t m_wordcount;
        
            bool compare( const float&, const float&, const comparison& ) const;
    };
    
    
    
    /** constructor
     * @param p_separator characters for seperate words within the text
     * @param p_casesensitive words should be case-sensitive / case-insensitive
     **/
    inline histogram::histogram( const std::string& p_separator, const bool& p_casesensitive ) :
        m_seperators( p_separator ),
        m_casesensitive( p_casesensitive ),
        m_map(),
        m_wordcount( 0 )
    {
        if (m_seperators.empty())
            throw exception::runtime(_("separator can not be empty"));
    }
    
    
    /** returns the value for case-sensitive words
     * @return bool of case-sensitive status
     **/
    inline bool histogram::getCaseSensitivity( void ) const
    {
        return m_casesensitive;
    }
    
    
    /** returns the number of words
     * @return number of words
     **/
    inline std::size_t histogram::getWordCount( void ) const
    {
        return m_wordcount;
    }
    

    /** adds a string vector to the map
     * @param p_vec string vector with data
     * @param p_minlen only word equal or greater will be added
     **/
    inline void histogram::add( const std::vector<std::string>& p_vec, const std::size_t& p_minlen )
    {
        for(std::size_t i=0; i < p_vec.size(); ++i)
            add( p_vec[i], p_minlen );
    }
    
    
    /** adds textdata to the histogram
     * @param p_text input text
     * @param p_minlen only word equal or greater will be added
     **/
    inline void histogram::add( const std::string& p_text, const std::size_t& p_minlen )
    {
        std::vector<std::string> l_data;
        boost::split( l_data, p_text, boost::is_any_of(m_seperators) );
        
        for(std::size_t i=0; i < l_data.size(); ++i) {
            std::string lc = l_data[i];
            if (lc.length() < p_minlen)
                continue;
                
            m_wordcount++;
            if (m_casesensitive)
                boost::to_lower(lc);
            
            std::map<std::string, std::size_t>::iterator it = m_map.find(lc);
            if (it == m_map.end())
                m_map[lc] = 1;
            else
                it->second++;
        }
    }
    
    
    /** returns a list of words, that between the ranges
     * @param p_val1 first value in range [0,1]
     * @param p_val2 second value in range [0,1]
     * @param p_comp1 comparasion operator of the first value
     * @param p_comp2 comparasion operator of the second value
     **/
    inline std::vector<std::string> histogram::getWords( const float& p_val1, const float& p_val2, const comparison& p_comp1, const comparison& p_comp2 )
    {
        if ( (p_val1 < 0) || (p_val1 > 1) || (p_val2 < 0) || (p_val2 > 1) )
            throw exception::runtime(_("ranges must be between [0,1]"));
        
        
        std::vector<std::string> l_list;
        
        for(std::map<std::string, std::size_t>::iterator it = m_map.begin(); it != m_map.end(); it++) {
            const float l_val = static_cast<float>(it->second) / m_wordcount;
            
            if ( (compare(l_val, p_val1, p_comp1)) || (compare(l_val, p_val2, p_comp2)) )
                  l_list.push_back( it->first );
        }
        
        return l_list;
    }
    
    
    /** returns a list of words, that in the range
     * @param p_val value in range [0,1]
     * @param p_comp comparasion operator of the value
     **/
    inline std::vector<std::string> histogram::getWords( const float& p_val, const comparison& p_comp )
    {
        if ( (p_val < 0) || (p_val > 1) )
            throw exception::runtime(_("ranges must be between [0,1]"));
        
        
        std::vector<std::string> l_list;
        
        for(std::map<std::string, std::size_t>::iterator it = m_map.begin(); it != m_map.end(); it++) {
            const float l_val = static_cast<float>(it->second) / m_wordcount;
            
            if (compare(l_val, p_val, p_comp))
                l_list.push_back( it->first );
        }
        
        return l_list;
    }
    
    
    /** compares two values with the operator
     * @param p_val1 value
     * @param p_val2 value
     * @param p_comp comparasion
     * @return bool
     **/
    inline bool histogram::compare( const float& p_val1, const float& p_val2, const comparison& p_comp ) const
    {
        bool ll = false;
        
        switch (p_comp) {
            case lessequal      : ll = p_val1 <= p_val2; break;
            case greaterequal   : ll = p_val1 >= p_val2; break;
            case less           : ll = p_val1 < p_val2;  break;
            case greater        : ll = p_val1 > p_val2;  break;
        }
        
        return ll;
    }

    
};};


#endif