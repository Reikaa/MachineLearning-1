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

#include <cstdlib>
#include <machinelearning.h>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#ifdef MACHINELEARNING_MPI
#include <boost/mpi.hpp>
#endif


namespace po        = boost::program_options;
namespace ublas     = boost::numeric::ublas;
namespace cluster   = machinelearning::clustering::nonsupervised;
namespace distance  = machinelearning::distances;
namespace tools     = machinelearning::tools;
#ifdef MACHINELEARNING_MPI
namespace mpi       = boost::mpi;
#endif



/** main program
 * @param argc number of arguments
 * @param argv arguments
 **/
int main(int argc, char* argv[])
{

    #ifdef MACHINELEARNING_MPI
    mpi::environment loMPIenv(argc, argv);
    mpi::communicator loMPICom;
    #endif
    
    #ifdef MACHINELEARNING_MULTILANGUAGE
    tools::language::bindings::bind("machinelearning", "language");
    #endif



    // default values
    bool l_log;
    std::size_t l_iteration;

    // create CML options with description
    po::options_description l_description("allowed options");
    l_description.add_options()
        ("help", "produce help message")
        ("outfile", po::value<std::string>(), "output HDF5 file")
        #ifdef MACHINELEARNING_MPI
        ("inputfile", po::value< std::vector<std::string> >()->multitoken(), "input HDF5 file")
        ("inputpath", po::value< std::vector<std::string> >()->multitoken(), "path to dataset")
        ("prototype", po::value< std::vector<std::size_t> >()->multitoken(), "number of prototypes")
        #else
        ("inputfile", po::value<std::string>(), "input HDF5 file")
        ("inputpath", po::value<std::string>(), "path to dataset")
        ("prototype", po::value<std::size_t>(), "number of prototypes")
        #endif
        ("iteration", po::value<std::size_t>(&l_iteration)->default_value(15), "number of iteration [default: 15]")
        ("log", po::value<bool>(&l_log)->default_value(false), "'true' for enable logging [default: false]")
    ;

    po::variables_map l_map;
    po::positional_options_description l_input;
    po::store(po::command_line_parser(argc, argv).options(l_description).positional(l_input).run(), l_map);
    po::notify(l_map);

    if (l_map.count("help")) {
        std::cout << l_description << std::endl;
        return EXIT_SUCCESS;
    }

    if ( (!l_map.count("outfile")) || (!l_map.count("inputfile")) || (!l_map.count("inputpath")) || (!l_map.count("prototype")) )
    {
        std::cerr << "[--outfile], [--inputfile], [--inputpath] and [--prototype] option must be set" << std::endl;
        return EXIT_FAILURE;
    }



    #ifdef MACHINELEARNING_MPI
    if(!(
         ((l_map["inputfile"].as< std::vector<std::string> >().size() == static_cast<std::size_t>(loMPICom.size())) && (l_map["inputpath"].as< std::vector<std::string> >().size() == 1)) ||
         ((l_map["inputpath"].as< std::vector<std::string> >().size() == static_cast<std::size_t>(loMPICom.size())) && (l_map["inputfile"].as< std::vector<std::string> >().size() == 1)) ||
         ((l_map["inputpath"].as< std::vector<std::string> >().size() == static_cast<std::size_t>(loMPICom.size())) && (l_map["inputfile"].as< std::vector<std::string> >().size() == static_cast<std::size_t>(loMPICom.size())))
         ))
        throw std::runtime_error("number of files or number of path must be equal to CPU rank");

    if (l_map["prototype"].as< std::vector<std::size_t> >().size() != static_cast<std::size_t>(loMPICom.size()))
        throw std::runtime_error("number of prototypes must be equal to CPU rank");
    #endif




    // read source hdf file and data
    #ifdef MACHINELEARNING_MPI
    const std::size_t filepos = l_map["inputefile"].as< std::vector<std::size_t> >().size() > 1 ? static_cast<std::size_t>(loMPICom.rank()) : 0;
    const std::size_t pathpos = l_map["inputepath"].as< std::vector<std::size_t> >().size() > 1 ? static_cast<std::size_t>(loMPICom.rank()) : 0;

    tools::files::hdf source( l_map["inputefile"].as< std::vector<std::string> >()[filepos] );
    ublas::matrix<double> data = source.readBlasMatrix<double>( l_map["inputpath"].as< std::vector<std::string> >()[pathpos], H5::PredType::NATIVE_DOUBLE);

    #else
    tools::files::hdf source( l_map["inputfile"].as<std::string>() );
    ublas::matrix<double> data = source.readBlasMatrix<double>( l_map["inputpath"].as<std::string>(), H5::PredType::NATIVE_DOUBLE);
    #endif

    // create distance object
    distance::euclid<double> d;


    #ifdef MACHINELEARNING_MPI
    cluster::neuralgas<double> ng(d, l_map["prototype"].as< std::vector<std::size_t> >()[static_cast<std::size_t>(loMPICom.rank())], data.size2());
    ng.setLogging( l_log );

    ng.train(loMPICom, data, l_iteration);

    // collect all data (of each process)
    ublas::matrix<double> protos = ng.getPrototypes(loMPICom);
    ublas::vector<double> qerror;
    std::vector< ublas::matrix<double> > logproto;

    if (ng.getLogging()) {
        qerror      = tools::vector::copy(ng.getLoggedQuantizationError(loMPICom));
        logproto    = ng.getLoggedPrototypes(loMPICom);
    }


    // only process 0 writes hdf
    if (loMPICom.rank() == 0) {
        tools::files::hdf target( l_map["outfile"].as<std::string>(), true);

        target.writeBlasMatrix<double>( "/protos",  protos, H5::PredType::NATIVE_DOUBLE );
        target.writeValue<double>( "/numprotos",  protos.size1(), H5::PredType::NATIVE_DOUBLE );
        target.writeValue<std::size_t>( "/iteration", l_iteration, H5::PredType::NATIVE_ULONG );

        if (ng.getLogging()) {
            target.writeBlasVector<double>( "/error", qerror, H5::PredType::NATIVE_DOUBLE );
            for(std::size_t i=0; i < logproto.size(); ++i)
                target.writeBlasMatrix<double>("/log" + boost::lexical_cast<std::string>( i )+"/protos", logproto[i], H5::PredType::NATIVE_DOUBLE );
        }
    }

    #else

    cluster::neuralgas<double> ng(d, l_map["prototype"].as<std::size_t>(), data.size2());
    ng.setLogging( l_log );

    ng.train(data, l_iteration);

    // create target file
    tools::files::hdf target(l_map["outfile"].as<std::string>(), true);
    target.writeValue<std::size_t>( "/numprotos",  l_map["prototype"].as<std::size_t>(), H5::PredType::NATIVE_ULONG );
    target.writeBlasMatrix<double>( "/protos",  ng.getPrototypes(), H5::PredType::NATIVE_DOUBLE );
    target.writeValue<std::size_t>( "/iteration",  l_iteration, H5::PredType::NATIVE_ULONG );

    if (ng.getLogging()) {
        target.writeBlasVector<double>( "/error",  tools::vector::copy(ng.getLoggedQuantizationError()), H5::PredType::NATIVE_DOUBLE );
        std::vector< ublas::matrix<double> > logproto =  ng.getLoggedPrototypes();
        for(std::size_t i=0; i < logproto.size(); ++i)
            target.writeBlasMatrix<double>("/log" + boost::lexical_cast<std::string>( i )+"/protos", logproto[i], H5::PredType::NATIVE_DOUBLE );
    }

    #endif


    #ifdef MACHINELEARNING_MPI
    if (loMPICom.rank() == 0) {
    #endif

    std::cout << "structure of the output file" << std::endl;
    std::cout << "/numprotos \t\t number of prototypes" << std::endl;
    std::cout << "/protos \t\t prototype matrix (row orientated)" << std::endl;
    std::cout << "/iteration \t\t number of iterations" << std::endl;

    if (ng.getLogging()) {
        std::cout << "/error \t\t quantization error on each iteration" << std::endl;
        std::cout << "/log<0 to number of iteration-1>/protosos \t\t prototypes on each iteration" << std::endl;
    }

    #ifdef MACHINELEARNING_MPI
    }
    #endif

    return EXIT_SUCCESS;
}
