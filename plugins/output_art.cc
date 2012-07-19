/*
 
 output_art.cc - This file is part of MUSIC -
 a code to generate multi-scale initial conditions 
 for cosmological simulations 
 
 Copyright (C) 2010  Oliver Hahn
 
 */
#include <sys/types.h>
#include <sys/stat.h>

#include "output.hh"


class art_output_plugin : public output_plugin
{
public:
	bool do_baryons_;
	double omegab_;
	double gamma_;
protected:
	
	typedef struct io_header
	{
		char head[45];
		float aexpN // current expansion factor
	        float aexp0 // initial expansion factor
		float amplt // Amplitude of density fluctuations
		float astep // Delta a -> time step. 
				// This value is also stored in pt.dat (binary 1 float)
				// It is recalculated by art so just a small value should work
		int istep // step (=0 in IC)
		int partw // mass of highest res particle.
        	float TINTG; //=0 in IC
        	float EKIN; //SUM 0.5 * m_i*(v_i**2) in code units
        	float EKIN1; //=0 in IC
        	float EKIN2; //=0 in IC
        	float AU0; //=0 in IC
        	float AEU0; //=0 in IC
        	int NROWC; // Number of particles in 1 dim (number of particles per page = NROW**2) 
	        int NGRIDC; // Number of cells in 1 dim
        	int nspecies; // number of dm species
	        int Nseed; // random number used ( 0 for MUSIC? or set the random number used in the lowest level?)
        	float Om0; //Omega_m
	        float Oml0; //Omega_L
        	float hubble; //hubble parameter
	        float Wp5; // 
        	float Ocurv; //Omega_k
	        //float Omb0; // this parameter only appears in header in hydro runs
		float wpart[10]; // extras[0-9] particle masses (normalized to low res particle)
		int lpart[10]; // extras[10-19] number of particles of each type
	        float extras[80]; //extras[20-99] 
		     //extras[9]=iLblock ->0 in IC 
                     //extras[10]=LevMin  ->0 in IC
                     //extras[11]=LevSmall ->0 in IC
                     //extras[12]=LevLarge ->0 in IC
                     //extras[13]=Omegab  ->0 in IC; fix it?
                     //extras[14]=sig8    ->0 in IC; fix it?
                     //extras[15]=Spslope ->0 in IC
                     //extras[16]=iDEswtch ->0 in IC
                     //extras[17]=DEw0    ->0 in IC
                     //extras[18]=DEwprime ->0 in IC
		     //extras[59]= 0 or 1; is used as switch for random numbers generators [do not apply in music use 0?]
		     //extras[60]= lux - level of luxury  [do not apply in music use 0?]
		     //extras[79]=Lbox (Mpc/h)

	}header;

	typedef struct io_ptf
	{
		float astep
	}ptf;
	
	header header_;
	ptf ptf_;
	std::string fname;
	size_t np_fine_gas_, np_fine_dm_, np_coarse_dm_;
	size_t block_buf_size_;
	size_t npartmax_;

public:


	art_output_plugin ( config_file& cf )
	: output_plugin( cf )
	{
		
		if( mkdir( fname_.c_str(), 0777 ) )
                {
                        perror( fname_.c_str() );
                        throw std::runtime_error("Error in art_output_plugin!");
                }

		do_baryons_ = cf.getValueSafe<bool>("setup","baryons",false);
		omegab_ = cf.getValueSafe<double>("cosmology","Omega_b",0.045);

		YHe_ = cf.getValueSafe<double>("cosmology","YHe",0.248);
                gamma_ = cf.getValueSafe<double>("cosmology","gamma",5.0/3.0);
		//... set time ......................................................
                header_.aexpn = 1.0/(1.0+header_.redshift);
                header_.aexp0 = header_.aexpn;
		//etc, etc

	}

	void write_header_file() //PMcrd.DAT
	{
		sprintf( filename, "%s/PMcrd.DAT", fname_.c_str() );
		ofs_.open(fname_.c_str(), std::ios::binary|std::ios::trunc );
		header this_header(header_);
		int blksize = sizeof(header); //529 in a dm only run; 533 in a baryon run
		ofs_.write( (char *)&blksize, sizeof(int) );
		ofs_.write( (char *)&this_header,sizeof(header));
		ofs_.write( (char *)&blksize, sizeof(int) );
		ofs_.close();
		
	}

	void write_pt_file() //pt.dat
	{
		sprintf( filename, "%s/pt.dat", fname_.c_str() );
		ofs_.open(fname_.c_str(), std::ios::binary|std::ios::trunc );
		ptf this_ptf(ptf_);
		int blksize = sizeof(ptf); //4
		ofs_.write( (char *)&blksize, sizeof(int) );
		ofs_.write( (char *)&this_ptf,sizeof(ptf));
		ofs_.write( (char *)&blksize, sizeof(int) );
		ofs_.close();
		
	}

	void write_dm_pages()
	{
		//The direct format write the particle data in pages.
		// Each page of particles is read into a common block,
		// which has the structure: X(Npage),Y(Npage),Z(Npage),
		// Vx(Npage),Vy(Npage),Vz(Npage). 
		///The number of particles in each page (Npage) is Npage = Nrow**2
		// There are NO Fortran size blocks pre or after these blocks!!
		//// Contradiction with documentation?? one file for each type of particle
		// however Daniel sent me just one file for a zoom. 
	}

	void finalize( void )
	{ 	}
};

namespace{
	output_plugin_creator_concrete<art_output_plugin> creator("art");
}
