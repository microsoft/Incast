// Incast
//
// Copyright (c) Microsoft Corporation
//
// All rights reserved. 
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#pragma once
#ifndef __HISTOGRAM_H_
#define __HISTOGRAM_H_

#include <map>
#include <unordered_map>
#include <string>
#include <sstream>
#include <limits>
#include <cmath>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

template< typename T >
class Histogram
{
    private:

    unsigned samples_;

#define USE_HASH_TABLE
#ifdef USE_HASH_TABLE
    std::unordered_map<T,unsigned> data_;

    std::map<T,unsigned> get_sorted_data() const
    {
        return std::map<T,unsigned>(data_.begin(), data_.end());
    }
#else
    std::map<T,unsigned> data_;

    std::map<T,unsigned> get_sorted_data() const
    {
        return data_; 
    }
#endif
    public: 

    Histogram()
        : samples_(0)
    {}

    void clear()
    {
        data_.clear();
        samples_ = 0;
    }

    void add( T v )
    { 
        data_[ v ]++;
        samples_++;
    }
	
    void merge( const Histogram<T> &other )
    {
        for( auto i : other.data_ )
        {
            data_[ i.first ] += i.second;
        }

        _samples += other._samples;
    }

    T get_min() const
    { 
        T min( std::numeric_limits<T>::max() );

        for( auto i : data_ )
        {
            if( i.first < min )
            {
                min = i.first;
            }
        }

        return min;
    }

    T get_max() const
    {
        T max( std::numeric_limits<T>::min() );

        for( auto i : data_ )
        {
            if( i.first > max ) 
            {
                max = i.first;
            }
        }

        return max;
    }
    
    unsigned get_sample_size() const 
    {
        return samples_;
    }
    
    T get_percentile( double p ) const 
    {
        // ISSUE-REVIEW
        // What do the 0th and 100th percentile really mean?
        if( (p < 0) || (p > 1) )
        {
            throw std::invalid_argument("Percentile must be >= 0 and <= 1");
        }

        const double target = get_sample_size() * p;

        unsigned cur = 0;
        for( auto i : get_sorted_data() ) 
        {
            cur += i.second;
            if( cur >= target )
            {
                return i.first;
            }
        }

        throw std::runtime_error("Percentile is undefined");
    }
    
    T get_percentile( int p ) const 
    {
        return get_percentile( static_cast<double>( p ) / 100 );
    }

    T get_median() const 
    { 
        return get_percentile( 0.5 ); 
    }

    
    double get_std_dev() const { return get_standard_deviation(); }
    double get_avg() const { return get_mean(); }

    double get_mean() const 
    {
        double sum(0);
	unsigned samples = get_sample_size();
        
        for( auto i : data_ )
        {
            double bucket_val =
                static_cast<double>(i.first) * i.second / samples;

            if (sum + bucket_val < 0)
            {
                throw std::overflow_error("while trying to accumulate sum");
            }

            sum += bucket_val;
        }

        return sum;
    }
    
    double get_standard_deviation() const
    {
        T mean(get_arithmetic_mean());
        T ssd(0);
        
        for( auto i : data_ )
        {
            double dev = static_cast<double>(i.first) - mean;
            double sqdev = dev*dev;
            ssd += i.second * sqdev;
        }

        return sqrt( ssd / get_sample_size() );
    }

    std::string get_histogram_csv( const unsigned BINS ) const
    {
        return get_histogram_csv( BINS, get_min(), get_max() );
    }

    std::string get_histogram_csv( const unsigned BINS, const T LOW, const T HIGH ) const
    {
        // ISSUE-REVIEW
        // Currently bins are defined as strictly less-than
        // their upper limit, with the exception of the last
        // bin.  Otherwise where would I put the max value?
        const double BIN_SIZE = (HIGH - LOW) / BINS;
        double limit = static_cast<double>(LOW);

        std::ostringstream os;
        os.precision(std::numeric_limits<T>::digits10);

        std::map<T,unsigned> sorted_data = get_sorted_data();

        auto pos = sorted_data.begin(); 

        unsigned cumulative = 0;

        for( unsigned bin = 1; bin <= BINS; ++bin )
        {
            unsigned count = 0;
            limit += BIN_SIZE;

            while( pos != sorted_data.end() && 
                ( pos->first < limit || bin == BINS ) )
            {
                count += pos->second;
                ++pos;
            }

            cumulative += count;

            os << limit << "," << count << "," << cumulative << std::endl;
        }

        return os.str();
    }

    std::string get_raw_csv() const
    {
        std::ostringstream os;
        os.precision(std::numeric_limits<T>::digits10);
        
        for( auto i : get_sorted_data() ) 
        {
            os << i.first << "," << i.second << std::endl;
        }
        
        return os.str();
    }

    std::string get_raw() const
    {
        std::ostringstream os;
        
        for( auto i : get_sorted_data() ) 
        {
            os << i.second << " " << i.first << std::endl;
        }

        return os.str();
    }
};

#pragma pop_macro("min")
#pragma pop_macro("max")

#endif
