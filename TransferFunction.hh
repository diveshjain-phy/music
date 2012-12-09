/*
 This file is part of MUSIC -
 a tool to generate initial conditions for cosmological simulations
 
 Copyright (C) 2008-12  Oliver Hahn, ojha@gmx.de
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __TRANSFERFUNCTION_HH
#define __TRANSFERFUNCTION_HH

#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_sf_gamma.h>

#include "Numerics.hh"
#include "general.hh"

#include <complex>

#define NZERO_Q

typedef std::complex<double> complex;

//! Abstract base class for transfer functions
/*!
    This class implements a purely virtual interface that can be
    used to derive instances implementing various transfer functions.
*/ 
class TransferFunction{
public:
	Cosmology m_Cosmology;
	
public:
  
	TransferFunction( Cosmology acosm ) : m_Cosmology( acosm ) { };
	virtual double compute( double k ) = 0;
	virtual ~TransferFunction(){ };
	virtual double get_kmax( void ) = 0;
	virtual double get_kmin( void ) = 0;
};

class TransferFunction_real
{
	
public:
	gsl_interp_accel *accp, *accn;
	gsl_spline *splinep, *splinen;
	double Tr0_, Tmin_, Tmax_, Tscale_;
	double rneg_, rneg2_;
	static TransferFunction *ptf_;
	static double nspec_;
	
protected:
	
	double krgood( double mu, double q, double dlnr, double kr )
	{
		double krnew = kr;
		complex cdgamma, zm, zp;
		double arg, iarg, xm, xp, y;
		gsl_sf_result g_a, g_p;
		
		xp = 0.5*(mu+1.0+q);
		xm = 0.5*(mu+1.0-q);
		y = M_PI/(2.0*dlnr);
		zp=complex(xp,y);
		zm=complex(xm,y);
		
		gsl_sf_lngamma_complex_e (zp.real(), zp.imag(), &g_a, &g_p);
		zp=std::polar(exp(g_a.val),g_p.val);
		double zpa = g_p.val;

		gsl_sf_lngamma_complex_e (zm.real(), zm.imag(), &g_a, &g_p);
		zm=std::polar(exp(g_a.val),g_p.val);
		double zma = g_p.val;
		
		arg=log(2.0/kr)/dlnr+(zpa+zma)/M_PI;
		iarg=(double)((int)(arg + 0.5));
		
		if( arg!=iarg )
			krnew=kr*exp((arg-iarg)*dlnr);
		
		return krnew;
	}
	
	void transform( double pnorm, double dplus, unsigned N, double q, std::vector<double>& rr, std::vector<double>& TT )
	{
		const double mu = 0.5;
		double qmin = 1.0e-6, qmax = 1.0e+6;
		
		q = 0.0;
		
		N = 16384;
		
#ifdef NZERO_Q
		//q = 0.4;
		q = 0.2;
#endif
		
		double kmin = qmin, kmax=qmax;
		double rmin = qmin, rmax = qmax;
		double k0 = exp(0.5*(log(kmax)+log(kmin)));
		double r0 = exp(0.5*(log(rmax)+log(rmin)));
		double L = log(rmax)-log(rmin);
		double k0r0 = k0*r0;
		double dlnk = L/N, dlnr = L/N;
		
		double sqrtpnorm = sqrt(pnorm);
		
		double dir = 1.0;
		
		double fftnorm = 1.0/N;
		
		fftw_complex in[N], out[N];
		fftw_plan p,ip;
		
		//... perform anti-ringing correction from Hamilton (2000)
		k0r0 = krgood( mu, q, dlnr, k0r0 );

		std::ofstream ofsk("transfer_k.txt");
		double sum_in = 0.0;
		for( unsigned i=0; i<N; ++i )
		{
			
			double k = k0*exp(((int)i - (int)N/2+1) * dlnk);
			//double k = k0*exp(((int)i - (int)N/2) * dlnk);
			//double k = k0*exp(ii * dlnk);
			
			//... some constants missing ...//
			in[i].re = dplus*sqrtpnorm*ptf_->compute( k )*pow(k,0.5*nspec_)*pow(k,1.5-q);
			in[i].im = 0.0;
			
			sum_in += in[i].re;
			ofsk << std::setw(16) << k <<std::setw(16) << in[i].re << std::endl;
		}
		ofsk.close();
		
		
		p = fftw_create_plan(N, FFTW_FORWARD, FFTW_ESTIMATE);
		ip = fftw_create_plan(N, FFTW_BACKWARD, FFTW_ESTIMATE);
		
		//fftw_one(p, in, out);
		fftw_one(p, in, out);
		
		//... compute the Hankel transform by convolution with the Bessel function
		for( unsigned i=0; i<N; ++i )
		{
			int ii=i;
			if( ii > (int)N/2 )
				ii -= N;
			
#ifndef NZERO_Q
			double y=ii*M_PI/L;
			complex zp((mu+1.0)*0.5,y);
			gsl_sf_result g_a, g_p;
			gsl_sf_lngamma_complex_e(zp.real(), zp.imag(), &g_a, &g_p);
			
			double arg = 2.0*(log(2.0/k0r0)*y+g_p.val);
			complex cu = complex(out[i].re,out[i].im)*std::polar(1.0,arg);
			out[i].re = cu.real()*fftnorm;
			out[i].im = cu.imag()*fftnorm;
			
#else		
			//complex x(dir*q, (double)ii*2.0*M_PI/L);
			complex x(dir*q, (double)ii*2.0*M_PI/L);
			gsl_sf_result g_a, g_p;
			
			complex g1, g2, garg, U, phase;						
			complex twotox = pow(complex(2.0,0.0),x);
			
			/////////////////////////////////////////////////////////
			//.. evaluate complex Gamma functions
			
			garg = 0.5*(mu+1.0+x);
			gsl_sf_lngamma_complex_e (garg.real(), garg.imag(), &g_a, &g_p);
			g1 = std::polar(exp(g_a.val),g_p.val);

			
			garg = 0.5*(mu+1.0-x);
			gsl_sf_lngamma_complex_e (garg.real(), garg.imag(), &g_a, &g_p);
			g2 = std::polar(exp(g_a.val),g_p.val);

			/////////////////////////////////////////////////////////
			//.. compute U
			
			if( (fabs(g2.real()) < 1e-19 && fabs(g2.imag()) < 1e-19) )
			{
				//std::cerr << "Warning : encountered possible singularity in TransferFunction_real::transform!\n";
				g1 = 1.0; g2 = 1.0;
			}
			
			
			U = twotox * g1 / g2;
			phase = pow(complex(k0r0,0.0),complex(0.0,2.0*M_PI*(double)ii/L));
			
			complex cu = complex(out[i].re,out[i].im)*U*phase*fftnorm;
			
			out[i].re = cu.real();
			out[i].im = cu.imag();
			
			if( (out[i].re != out[i].re)||(out[i].im != out[i].im) )
			{	std::cerr << "NaN @ i=" << i << ", U= " << U << ", phase = " << phase << ", g1 = " << g1 << ", g2 = " << g2 << std::endl;
				std::cerr << "mu+1+q = " << mu+1.0+q << std::endl;
				//break;
			}
			
#endif

		}
			
		/*out[N/2].im = 0.0;
		out[N/2+1].im = 0.0;
		out[N/2+1].re = out[N/2].re;
		out[N/2].im = 0.0;*/
		
		fftw_one(ip, out, in);
		
		rr.assign(N,0.0);
		TT.assign(N,0.0);
		
		r0 = k0r0/k0;
		
		for( unsigned i=0; i<N; ++i )
		{
			int ii = i;
			ii -= N/2-1;
			//ii -= N/2;
			//if( ii>N/2)
			//	ii-=N;
			
			
			
			double r = r0*exp(-ii*dlnr);
			rr[N-i-1] = r;
			TT[N-i-1] = 4.0*M_PI* sqrt(M_PI/2.0) *  in[i].re*pow(r,-(1.5+q));
			
			//TT[N-i-1] = 4.0*M_PI* sqrt(M_PI/2.0) *  in[i].re*exp( -dir*(q+1.5)*ii*dlnr +q*log(k0r0))/r0;
			
			//rr[i] = r;
			//TT[i] = 4.0*M_PI* sqrt(M_PI/2.0) *  in[i].re*pow(r,-(1.5+q));
			
		}
		
		
		{
			std::ofstream ofs("transfer_real_new.txt");
			for( unsigned i=0; i<N; ++i )
			{
				int ii = i;
				ii -= N/2-1;
				
				double r = r0*exp(-ii*dlnr);//r0*exp(ii*dlnr);
				double T = 4.0*M_PI* sqrt(M_PI/2.0) *  in[i].re*pow(r,-(1.5+q));
				ofs << r << "\t\t" << T << "\t\t" << in[i].im << std::endl;
			}
		}
		

		fftw_destroy_plan(p);
		fftw_destroy_plan(ip);
	}
	
public:
	TransferFunction_real( TransferFunction *tf, double nspec, double pnorm, double dplus, double rmin, double rmax, double knymax, unsigned nr )
	{
				
		ptf_ = tf;
		nspec_ = nspec;
	
		double q = 0.8;
		
		std::vector<double> r,T,xp,yp,xn,yn;
		
		transform( pnorm, dplus, nr, q, r, T );
		
		//... determine r=0 zero component by integrating up to the Nyquist frequency
		gsl_integration_workspace * wp; 
		gsl_function F;
		wp = gsl_integration_workspace_alloc(20000);
		F.function = &call_wrapper;
		double par[2]; par[0] = dplus*sqrt(pnorm); //par[1] = M_PI/kny;
		F.params = (void*)par;
		double error;
		
		//#warning factor of sqrt(1.5) needs to be adjusted for non-equilateral boxes
		//.. need a factor sqrt( 2*kny^2_x + 2*kny^2_y + 2*kny^2_z )/2 = sqrt(3/2)kny (in equilateral case)
		gsl_integration_qag (&F, 0.0, sqrt(1.5)*knymax, 0, 1e-8, 20000, GSL_INTEG_GAUSS21, wp, &Tr0_, &error); 
		//Tr0_ = 0.0;
		gsl_integration_workspace_free(wp);
				
		
		for( unsigned i=0; i<r.size(); ++i )
		{
			// spline positive and negative part separately
			/*if( T[i] > 0.0 )
			{
				xp.push_back( 2.0*log10(r[i]) );
				yp.push_back( log10(T[i]) );
				rneg_ = r[i];
				rneg2_ = rneg_*rneg_;
			}else {
				xn.push_back( 2.0*log10(r[i]) );
				yn.push_back( log10(-T[i]) );
			}*/
			
			
			if( r[i] > rmin && r[i] < rmax )
			{
				xp.push_back( 2.0*log10(r[i]) );
				yp.push_back( log10(fabs(T[i])) );
				xn.push_back( 2.0*log10(r[i]) );
				if( T[i] >= 0.0 ) 
					yn.push_back( 1.0 );
				else
					yn.push_back( -1.0 );
				
				
				//ofs << std::setw(16) << xp.back() << std::setw(16) << yp.back() << std::endl;
			}
			
		}
		

		
		
		
		accp = gsl_interp_accel_alloc ();
		accn = gsl_interp_accel_alloc ();
		
		//... spline interpolation is only marginally slower here
		splinep = gsl_spline_alloc (gsl_interp_cspline, xp.size() );
		splinen = gsl_spline_alloc (gsl_interp_cspline, xn.size() );

		//... set up everything for spline interpolation
		gsl_spline_init (splinep, &xp[0], &yp[0], xp.size() );
		gsl_spline_init (splinen, &xn[0], &yn[0], xn.size() );		
		

		
		
		{
			double dlogr = (log10(rmax)-log10(rmin))/100;
			std::ofstream ofs("transfer_splinep.txt");			
			
			for( int i=0; i< 100; ++i ) 
			{
				double r = rmin*pow(10.0,i*dlogr);
				ofs << std::setw(16) << r << std::setw(16) << compute_real(r*r) << std::endl;
			}
		}
		
	}
	
	static double call_wrapper( double k, void *arg )
	{
		double *a = (double*)arg;
		return 4.0*M_PI*a[0]*ptf_->compute( k )*pow(k,0.5*nspec_)*k*k;
	}
	
	~TransferFunction_real()
	{
		gsl_spline_free (splinen);
		gsl_interp_accel_free (accn);
		gsl_spline_free (splinep);
		gsl_interp_accel_free (accp);

	}
	
	inline double compute_real( double r2 ) const
	{
		const double EPS = 1e-8;
		const double Reps2 = EPS*EPS;
		
		if( r2 <Reps2 )
			return Tr0_;
		double q;
		/*if( r2 < rneg2_ )
			q = pow(10.0,gsl_spline_eval (splinep, log10(r2), accp));
		else
			q = -pow(10.0,gsl_spline_eval(splinen, log10(r2), accn));*/
		
		double logr2 = log10(r2);
		q = pow(10.0,gsl_spline_eval(splinep, logr2, accp));
		double sign = 1.0;
		if( gsl_spline_eval(splinen, logr2, accn) < 0.0 )
			sign = -1.0;
		return q*sign;
	}
};

class TransferFunction_real_old
{
protected:
	bool m_breal_init;
	static double nspec;
	gsl_integration_workspace * wp; 
	gsl_integration_workspace * c_wp;
	gsl_integration_qawo_table * wf;
	gsl_function F;
	
	gsl_interp_accel *acc;
	gsl_spline *spline;
	
	static TransferFunction *ptf;
	
public:
	TransferFunction_real_old( TransferFunction *tf, double ns, double pnorm, double dplus, double rmin, double rmax, unsigned nr )
	: m_breal_init(false)
	{ 
		
		ptf = tf;
		nspec = ns;
		wf = gsl_integration_qawo_table_alloc(0.0, 10000.0, GSL_INTEG_SINE, 10000);            
		
		
		wp = gsl_integration_workspace_alloc(20000);
		c_wp = gsl_integration_workspace_alloc(20000);
		
		acc = gsl_interp_accel_alloc ();
		spline = gsl_spline_alloc (gsl_interp_cspline, nr+1);
		
		double dlogr = (log10(rmax)-log10(rmin))/nr;
		
		std::vector<double> x(nr+1,0.0),y(nr+1,0.0);
		
		
		/*std::ofstream ofs("transfer_fourier.txt");
		for( int i=0; i<1000; ++i )
		{
			double k=1.e-3*pow(10.0,i*6.0/1000.0);
			ofs << k << "\t\t" << call_wrapper(k, NULL) << std::endl;
		}*/
		

		double fac = sqrt(pnorm)*dplus;
		
		std::ofstream ofs("transfer_real_old.txt");
		
		for( unsigned i=0; i<nr+1; ++i )
		{
			double r = rmin*pow(10.0,(double)i*dlogr);
			double result1, result2, error;
			
			//... integrate from zero to first point ...//
			double k0 = 1e-2;

			F.function = &call_wrapper2;
			double k[1]; k[0] = r;
			F.params = (void*)k;
			
			gsl_integration_qags (&F, 0.0, k0, 0, 1e-7, 20000,  wp, &result1, &error); 
			
			//... integrate further ...//
			F.function = &call_wrapper;
			F.params = NULL;

			//std::cerr << "r = " << r << std::endl; 

			gsl_integration_qawo_table_set(wf,r,100000.0,GSL_INTEG_SINE);
			gsl_integration_qawf(&F,k0,1e-8,20000,wp,c_wp,wf,&result2,&error);
			
			result1 = 4.0*M_PI*fac*(result1+result2)/r;
			
			x[i] = log10(r*r);
			y[i] = log10(result1);
			
			ofs << std::setw(16) << r << std::setw(16) << result1 << std::endl;
			
		}
		
		gsl_spline_init (spline, &x[0], &y[0], nr+1);
		
		gsl_integration_qawo_table_free(wf);
		gsl_integration_workspace_free(wp);
		gsl_integration_workspace_free(c_wp);
		
	}
	
	~TransferFunction_real_old()
	{
		gsl_spline_free (spline);
		gsl_interp_accel_free (acc);
	}
	
	static double call_wrapper( double k, void *arg )
	{
		//return ptf->compute( k );
		return ptf->compute( k )*k*pow(k,0.5*nspec);
		//return ptf->compute(k);
	}
	
	static double call_wrapper2( double k, void *arg )
	{
		//return ptf->compute( k );
		double r = ((double*)arg)[0];
		return ptf->compute( k )*k*pow(k,0.5*nspec) *sin(k*r);
		//return ptf->compute(k);
	}
	
	double compute_real( double r2 )
	{
		if( r2 < 1e-2 )
			return 0.0;
		return pow(10.0,gsl_spline_eval (spline, log10(r2), acc));
	}
};

//! Implementation of class TransferFunction_tab for the BBKS transfer function 
/*!
    
*/
class TransferFunction_tab : public TransferFunction{
private:
  //Cosmology m_Cosmology;

  std::string m_filename;
  std::vector<double> m_tab_k;
  std::vector<double> m_tab_Tk;
	

  void read_table( void ){
#ifdef WITH_MPI
    if( MPI::COMM_WORLD.Get_rank() == 0 ){
#endif
      std::cerr << " - reading tabulated transfer function from file \'"
                << m_filename << "\'\n";
      std::string line;
      std::ifstream ifs( m_filename.c_str() );
      
      m_tab_k.clear();
      m_tab_Tk.clear();
      
      while( !ifs.eof() ){
        getline(ifs,line);
        std::stringstream ss(line);
        
        double k, Tk;
        ss >> k;
        ss >> Tk;
        
        m_tab_k.push_back( k );
        m_tab_Tk.push_back( Tk );
        
      }
#ifdef WITH_MPI
    }
    
    unsigned n=m_tab_k.size();
    MPI::COMM_WORLD.Bcast( &n, 1, MPI_UNSIGNED, 0 );
    
    if( MPI::COMM_WORLD.Get_rank() > 0 ){
      m_tab_k.assign(n,0);
      m_tab_Tk.assign(n,0);
    }
    
    MPI::COMM_WORLD.Bcast( &m_tab_k[0],  n, MPI_DOUBLE, 0 );
    MPI::COMM_WORLD.Bcast( &m_tab_Tk[0], n, MPI_DOUBLE, 0 );
#endif
    
  }

public:
  TransferFunction_tab( Cosmology aCosm, std::string filename )
  : TransferFunction( aCosm ), m_filename( filename )
  {
    read_table();
  }
  
  virtual inline double compute( double k ){
    
    return linint( k, m_tab_k, m_tab_Tk );
  }
  
  inline double get_kmin( void ){
    return m_tab_k[1];
  }
  
  inline double get_kmax( void ){
    return m_tab_k[m_tab_k.size()-2];
  }
  
};

class TransferFunction_CAMB : public TransferFunction{
public:
	enum TFtype{
		TF_baryon, TF_cdm, TF_total
	};
	
	
private:
	//Cosmology m_Cosmology;
	
	std::string m_filename_Pk, m_filename_Tk;
	std::vector<double> m_tab_k, m_tab_Tk;
	
	Spline_interp *m_psinterp;
	gsl_interp_accel *acc;
	gsl_spline *spline;
	
	
	
	void read_table( TFtype iwhich ){
#ifdef WITH_MPI
		if( MPI::COMM_WORLD.Get_rank() == 0 ){
#endif
			std::cerr 
				<< " - reading tabulated transfer function data from file \n"
				<< "    \'" << m_filename_Tk << "\'\n";
			
			std::string line;
			std::ifstream ifs( m_filename_Tk.c_str() );
			
			m_tab_k.clear();
			m_tab_Tk.clear();
			
			while( !ifs.eof() ){
				getline(ifs,line);
				
				if(ifs.eof()) break;
				
				std::stringstream ss(line);
				
				double k, Tkc, Tkb, Tkg, Tkr, Tknu, Tktot;
				ss >> k;
				ss >> Tkc;
				ss >> Tkb;
				ss >> Tkg;
				ss >> Tkr;
				ss >> Tknu;
				ss >> Tktot;
				
				m_tab_k.push_back( log10(k) );
				
				switch( iwhich ){
					case TF_total:
						m_tab_Tk.push_back( log10(Tktot) );
						break;
					case TF_baryon:
						m_tab_Tk.push_back( log10(Tkb) );
						break;
					case TF_cdm:
						m_tab_Tk.push_back( log10(Tkc) );
						break;
				}
				//m_tab_Tk_cdm.push_back( Tkc );
				//m_tab_Tk_baryon.push_back( Tkb );
				//m_tab_Tk_tot.push_back( Tktot );
			}
			
			ifs.close();
			
			
			
			
#ifdef WITH_MPI
		}
		
		unsigned n=m_tab_k.size();
		MPI::COMM_WORLD.Bcast( &n, 1, MPI_UNSIGNED, 0 );
		
		if( MPI::COMM_WORLD.Get_rank() > 0 ){
			m_tab_k.assign(n,0);
			m_tab_Tk.assign(n,0);
		}
		
		MPI::COMM_WORLD.Bcast( &m_tab_k[0],  n, MPI_DOUBLE, 0 );
		MPI::COMM_WORLD.Bcast( &m_tab_Tk[0], n, MPI_DOUBLE, 0 );
#endif
		
	}
	
public:
	TransferFunction_CAMB( Cosmology aCosm, std::string filename_Tk, TFtype iwhich )
	: TransferFunction( aCosm ), m_filename_Tk( filename_Tk ), m_psinterp( NULL )
	{
		read_table( iwhich );
		
		//m_psinterp = new Spline_interp( m_tab_k, m_tab_Tk );
		
		acc = gsl_interp_accel_alloc();
		//spline = gsl_spline_alloc( gsl_interp_cspline, m_tab_k.size() );
		spline = gsl_spline_alloc( gsl_interp_akima, m_tab_k.size() );
		
		gsl_spline_init (spline, &m_tab_k[0], &m_tab_Tk[0], m_tab_k.size() );
		
	}
	
	virtual ~TransferFunction_CAMB()
	{
		//delete m_psinterp;
		
		gsl_spline_free (spline);
		gsl_interp_accel_free (acc);
	}
	
	virtual inline double compute( double k ){
		//double Tkc = linint( k, m_tab_k, m_tab_Tk_cdm );
		//double Tkb = linint( k, m_tab_k, m_tab_Tk_baryon );
		
		//return pow(10.0, linint( log10(k), m_tab_k, m_tab_Tk ));
		
		//return pow(10.0, m_psinterp->interp(log10(k)) );
		
		//if( k<get_kmin() )
		//	return pow(10.0,m_tab_k[1]);
		
		return pow(10.0, gsl_spline_eval (spline, log10(k), acc) );
	}
	
	inline double get_kmin( void ){
		return pow(10.0,m_tab_k[1]);
	}
	
	inline double get_kmax( void ){
		return pow(10.0,m_tab_k[m_tab_k.size()-2]);
	}
	
};


//! Implementation of class TransferFunction_BBKS for the BBKS transfer function 
/*!
    This class implements the analytical fit to the matter transfer
    function by Bardeen, Bond, Kaiser & Szalay (BBKS).
    ( see Bardeen et al. (1986) )
*/
class TransferFunction_BBKS : public TransferFunction{
private:
  //Cosmology m_Cosmology;
  double      m_Gamma;

public:
  //! Constructor
  /*!
    \param aCosm Structure of type Cosmology carrying the cosmological parameters
	\param bSugiyama flag whether the Sugiyama (1995) correction shall be applied (default=true)
  */
  TransferFunction_BBKS( Cosmology aCosm, bool bSugiyama=true, double FreeGamma=-1.0 )
    : TransferFunction( aCosm )
  {  
    double Omega0 = m_Cosmology.Omega_m;
	  
    if( FreeGamma <= 0.0 ){
      m_Gamma = Omega0*0.01*m_Cosmology.H0;
      if( bSugiyama )
        m_Gamma *= exp(-m_Cosmology.Omega_b*(1.0+sqrt(2.0*0.01*m_Cosmology.H0)/Omega0));
    }else
      m_Gamma = FreeGamma;
	
	
  }

  //! computes the value of the BBKS transfer function for mode k (in h/Mpc)
  virtual inline double compute( double k ){
    double q, f1, f2;
    
    q = k/(m_Gamma);
    f1 = log(1.0 + 2.34*q)/(2.34*q);
    f2 = 1.0 + q*(3.89 + q*(259.21 + q*(162.771336 + q*2027.16958081)));
    
    return f1/sqrt(sqrt(f2));

  }
  
  inline double get_kmin( void ){
    return 0.0;
  }
  
  inline double get_kmax( void ){
    return 1.e3;
  }
};

//! Implementation of abstract base class TransferFunction for the Eisenstein & Hu transfer function 
/*!
    This class implements the analytical fit to the matter transfer
    function by Eisenstein & Hu (1999).
*/
class TransferFunction_Eisenstein : public TransferFunction{
protected:
  //Cosmology m_Cosmology;
  double  m_h0;
  double	omhh,		/* Omega_matter*h^2 */
	obhh,		/* Omega_baryon*h^2 */
	theta_cmb,	/* Tcmb in units of 2.7 K */
	z_equality,	/* Redshift of matter-radiation equality, really 1+z */
	k_equality,	/* Scale of equality, in Mpc^-1 */
	z_drag,		/* Redshift of drag epoch */
	R_drag,		/* Photon-baryon ratio at drag epoch */
	R_equality,	/* Photon-baryon ratio at equality epoch */
	sound_horizon,	/* Sound horizon at drag epoch, in Mpc */
	k_silk,		/* Silk damping scale, in Mpc^-1 */
	alpha_c,	/* CDM suppression */
	beta_c,		/* CDM log shift */
	alpha_b,	/* Baryon suppression */
	beta_b,		/* Baryon envelope shift */
	beta_node,	/* Sound horizon shift */
	k_peak,		/* Fit to wavenumber of first peak, in Mpc^-1 */
	sound_horizon_fit,	/* Fit to sound horizon, in Mpc */
	alpha_gamma;	/* Gamma suppression in approximate TF */

  //! private member function: sets internal quantities for Eisenstein & Hu fitting
  void TFset_parameters(double omega0hh, double f_baryon, double Tcmb)
  /* Set all the scalars quantities for Eisenstein & Hu 1997 fitting formula */
  /* Input: omega0hh -- The density of CDM and baryons, in units of critical dens,
     multiplied by the square of the Hubble constant, in units
     of 100 km/s/Mpc */
  /* 	  f_baryon -- The fraction of baryons to CDM */
  /*        Tcmb -- The temperature of the CMB in Kelvin.  Tcmb<=0 forces use
	    of the COBE value of  2.728 K. */
  /* Output: Nothing, but set many global variables used in TFfit_onek(). 
     You can access them yourself, if you want. */
  /* Note: Units are always Mpc, never h^-1 Mpc. */
  {
    double z_drag_b1, z_drag_b2;
    double alpha_c_a1, alpha_c_a2, beta_c_b1, beta_c_b2, alpha_b_G, y;
	  
    if (f_baryon<=0.0 || omega0hh<=0.0) {
      fprintf(stderr, "TFset_parameters(): Illegal input.\n");
      exit(1);
    }
    omhh = omega0hh;
    obhh = omhh*f_baryon;
    if (Tcmb<=0.0) Tcmb=2.728;	/* COBE FIRAS */
    theta_cmb = Tcmb/2.7;
    
    z_equality = 2.50e4*omhh/POW4(theta_cmb);  /* Really 1+z */
    k_equality = 0.0746*omhh/SQR(theta_cmb);

    z_drag_b1 = 0.313*pow((double)omhh,-0.419)*(1+0.607*pow((double)omhh,0.674));
    z_drag_b2 = 0.238*pow((double)omhh,0.223);
    z_drag = 1291*pow(omhh,0.251)/(1+0.659*pow((double)omhh,0.828))*
		(1+z_drag_b1*pow((double)obhh,(double)z_drag_b2));
    
    R_drag = 31.5*obhh/POW4(theta_cmb)*(1000/(1+z_drag));
    R_equality = 31.5*obhh/POW4(theta_cmb)*(1000/z_equality);

    sound_horizon = 2./3./k_equality*sqrt(6./R_equality)*
	    log((sqrt(1+R_drag)+sqrt(R_drag+R_equality))/(1+sqrt(R_equality)));

    k_silk = 1.6*pow((double)obhh,0.52)*pow((double)omhh,0.73)*(1+pow((double)10.4*omhh,-0.95));

    alpha_c_a1 = pow((double)46.9*omhh,0.670)*(1+pow(32.1*omhh,-0.532));
    alpha_c_a2 = pow((double)12.0*omhh,0.424)*(1+pow(45.0*omhh,-0.582));
    alpha_c = pow(alpha_c_a1,-f_baryon)*
		pow(alpha_c_a2,-CUBE(f_baryon));
    
    beta_c_b1 = 0.944/(1+pow(458*omhh,-0.708));
    beta_c_b2 = pow(0.395*omhh, -0.0266);
    beta_c = 1.0/(1+beta_c_b1*(pow(1-f_baryon, beta_c_b2)-1));

    y = z_equality/(1+z_drag);
    alpha_b_G = y*(-6.*sqrt(1+y)+(2.+3.*y)*log((sqrt(1+y)+1)/(sqrt(1+y)-1)));
    alpha_b = 2.07*k_equality*sound_horizon*pow(1+R_drag,-0.75)*alpha_b_G;

    beta_node = 8.41*pow(omhh, 0.435);
    beta_b = 0.5+f_baryon+(3.-2.*f_baryon)*sqrt(pow(17.2*omhh,2.0)+1);

    k_peak = 2.5*3.14159*(1+0.217*omhh)/sound_horizon;
    sound_horizon_fit = 44.5*log(9.83/omhh)/sqrt(1+10.0*pow(obhh,0.75));

    alpha_gamma = 1-0.328*log(431.0*omhh)*f_baryon + 0.38*log(22.3*omhh)*
		SQR(f_baryon);
    
    return;
  }

  //! private member function: computes transfer function for mode k (k in Mpc)
  inline double TFfit_onek(double k, double *tf_baryon, double *tf_cdm)
  /* Input: k -- Wavenumber at which to calculate transfer function, in Mpc^-1.
   *tf_baryon, *tf_cdm -- Input value not used; replaced on output if
   the input was not NULL. */
  /* Output: Returns the value of the full transfer function fitting formula.
     This is the form given in Section 3 of Eisenstein & Hu (1997).
     *tf_baryon -- The baryonic contribution to the full fit.
     *tf_cdm -- The CDM contribution to the full fit. */
  /* Notes: Units are Mpc, not h^-1 Mpc. */
  {
    double T_c_ln_beta, T_c_ln_nobeta, T_c_C_alpha, T_c_C_noalpha;
    double q, xx, xx_tilde;//, q_eff;
    double T_c_f, T_c, s_tilde, T_b_T0, T_b, f_baryon, T_full;
    //double T_0_L0, T_0_C0, T_0, gamma_eff; 
    //double T_nowiggles_L0, T_nowiggles_C0, T_nowiggles;

    k = fabs(k);	/* Just define negative k as positive */
    if (k==0.0) {
	if (tf_baryon!=NULL) *tf_baryon = 1.0;
	if (tf_cdm!=NULL) *tf_cdm = 1.0;
	return 1.0;
    }

    q = k/13.41/k_equality;
    xx = k*sound_horizon;

    T_c_ln_beta = log(2.718282+1.8*beta_c*q);
    T_c_ln_nobeta = log(2.718282+1.8*q);
    T_c_C_alpha = 14.2/alpha_c + 386.0/(1+69.9*pow(q,1.08));
    T_c_C_noalpha = 14.2 + 386.0/(1+69.9*pow(q,1.08));

    T_c_f = 1.0/(1.0+POW4(xx/5.4));
    T_c = T_c_f*T_c_ln_beta/(T_c_ln_beta+T_c_C_noalpha*SQR(q)) +
	    (1-T_c_f)*T_c_ln_beta/(T_c_ln_beta+T_c_C_alpha*SQR(q));
    
    s_tilde = sound_horizon*pow(1+CUBE(beta_node/xx),-1./3.);
    xx_tilde = k*s_tilde;

    T_b_T0 = T_c_ln_nobeta/(T_c_ln_nobeta+T_c_C_noalpha*SQR(q));
    T_b = sin(xx_tilde)/(xx_tilde)*(T_b_T0/(1+SQR(xx/5.2))+
		alpha_b/(1+CUBE(beta_b/xx))*exp(-pow(k/k_silk,1.4)));
    
    f_baryon = obhh/omhh;
    T_full = f_baryon*T_b + (1-f_baryon)*T_c;

    /* Now to store these transfer functions */
    if (tf_baryon!=NULL) *tf_baryon = T_b;
    if (tf_cdm!=NULL) *tf_cdm = T_c;
    return T_full;
}

public:
  //! Constructor for Eisenstein & Hu fitting for transfer function
  /*!
    \param aCosm structure of type Cosmology carrying the cosmological parameters
    \param Tcmb mean temperature of the CMB fluctuations (defaults to
    Tcmb = 2.726 if not specified)
  */
  TransferFunction_Eisenstein( Cosmology aCosm, double Tcmb = 2.726 )
    :  TransferFunction(aCosm), m_h0( aCosm.H0*0.01 )
  {
    TFset_parameters( (aCosm.Omega_m)*aCosm.H0*aCosm.H0*(0.01*0.01), 
		      aCosm.Omega_b/(aCosm.Omega_m-aCosm.Omega_b),//-aCosm.Omega_b), 
		    Tcmb);
  }

  //! Computes the transfer function for k in Mpc/h by calling TFfit_onek
  virtual inline double compute( double k ){
    double tfb, tfcdm, fb, fc; //, tfull
    TFfit_onek( k*m_h0, &tfb, &tfcdm );
    
    fb = m_Cosmology.Omega_b/(m_Cosmology.Omega_m);
    fc = (m_Cosmology.Omega_m-m_Cosmology.Omega_b)/(m_Cosmology.Omega_m) ;
    
    return fb*tfb+fc*tfcdm;
    //return 1.0;
  }
	
	
  inline void compute( double k, double &tf_dm, double &tf_baryon ){
	double tfb, tfcdm, fb, fc; //, tfull
	TFfit_onek( k*m_h0, &tfb, &tfcdm );
	  
	fb = m_Cosmology.Omega_b/(m_Cosmology.Omega_m);
	fc = (m_Cosmology.Omega_m-m_Cosmology.Omega_b)/(m_Cosmology.Omega_m) ;
	  
	tf_dm = fc*tfcdm;
	tf_baryon = fb*tfb;
  }
  
	
  inline double get_kmin( void ){
    return 0.0;
  }
  
  inline double get_kmax( void ){
    return 1.e3;
  }

};



class TransferFunction_EisensteinWDM : public TransferFunction_Eisenstein
{
protected:
	using TransferFunction_Eisenstein::TFfit_onek;
	double m_WDMalpha; 
	
public:
	TransferFunction_EisensteinWDM( Cosmology aCosm, double Tcmb = 2.726 )
	: TransferFunction_Eisenstein(aCosm,Tcmb)
	{
		m_WDMalpha = 0.05 * pow( aCosm.Omega_m/0.4,0.15)
					*pow(aCosm.H0/0.65,1.3)*pow(aCosm.WDMmass,-1.15)
					*pow(1.5/aCosm.WDMg_x,0.29);
	}
	
	inline double compute( double k )
	{
		double tfb, tfcdm, fb, fc; //, tfull
		TFfit_onek( k*m_h0, &tfb, &tfcdm );
		
		fb = m_Cosmology.Omega_b/(m_Cosmology.Omega_m);
		fc = (m_Cosmology.Omega_m-m_Cosmology.Omega_b)/(m_Cosmology.Omega_m) ;
	
		double tf = fb*tfb+fc*tfcdm;
		
		return tf*pow(1.0+(m_WDMalpha*k)*(m_WDMalpha*k),-5.0);
		
	}
	
};




class TransferFunction_EisensteinNeutrino : public TransferFunction
{
	/* Fitting Formulae for CDM + Baryon + Massive Neutrino (MDM) cosmologies. */
	/* Daniel J. Eisenstein & Wayne Hu, Institute for Advanced Study */
	
	/* There are two primary routines here, one to set the cosmology, the
	 other to construct the transfer function for a single wavenumber k. 
	 You should call the former once (per cosmology) and the latter as 
	 many times as you want. */
	
	/* TFmdm_set_cosm() -- User passes all the cosmological parameters as
	 arguments; the routine sets up all of the scalar quantites needed 
	 computation of the fitting formula.  The input parameters are: 
	 1) omega_matter -- Density of CDM, baryons, and massive neutrinos,
	 in units of the critical density. 
	 2) omega_baryon -- Density of baryons, in units of critical. 
	 3) omega_hdm    -- Density of massive neutrinos, in units of critical 
	 4) degen_hdm    -- (Int) Number of degenerate massive neutrino species 
	 5) omega_lambda -- Cosmological constant 
	 6) hubble       -- Hubble constant, in units of 100 km/s/Mpc 
	 7) redshift     -- The redshift at which to evaluate */
	
	/* TFmdm_onek_mpc() -- User passes a single wavenumber, in units of Mpc^-1.
	 Routine returns the transfer function from the Eisenstein & Hu
	 fitting formula, based on the cosmology currently held in the 
	 internal variables.  The routine returns T_cb (the CDM+Baryon
	 density-weighted transfer function), although T_cbn (the CDM+
	 Baryon+Neutrino density-weighted transfer function) is stored
	 in the global variable tf_cbnu. */
	
	/* We also supply TFmdm_onek_hmpc(), which is identical to the previous
	 routine, but takes the wavenumber in units of h Mpc^-1. */
	
	/* We hold the internal scalar quantities in global variables, so that
	 the user may access them in an external program, via "extern" declarations. */
	
	/* Please note that all internal length scales are in Mpc, not h^-1 Mpc! */
	
	/* -------------------------- Prototypes ----------------------------- */
	
/*	int TFmdm_set_cosm(float omega_matter, float omega_baryon, float omega_hdm,
					   int degen_hdm, float omega_lambda, float hubble, float redshift);
	float TFmdm_onek_mpc(float kk);
	float TFmdm_onek_hmpc(float kk);
*/	
	
	/* ------------------------- Global Variables ------------------------ */
	
	/* The following are set in TFmdm_set_cosm() */
	float   alpha_gamma,	/* sqrt(alpha_nu) */
	alpha_nu,	/* The small-scale suppression */
	beta_c,		/* The correction to the log in the small-scale */
	num_degen_hdm,	/* Number of degenerate massive neutrino species */
	f_baryon,	/* Baryon fraction */
	f_bnu,		/* Baryon + Massive Neutrino fraction */
	f_cb,		/* Baryon + CDM fraction */
	f_cdm,		/* CDM fraction */
	f_hdm,		/* Massive Neutrino fraction */
	growth_k0,	/* D_1(z) -- the growth function as k->0 */
	growth_to_z0,	/* D_1(z)/D_1(0) -- the growth relative to z=0 */
	hhubble,	/* Need to pass Hubble constant to TFmdm_onek_hmpc() */
	k_equality,	/* The comoving wave number of the horizon at equality*/
	obhh,		/* Omega_baryon * hubble^2 */
	omega_curv,	/* = 1 - omega_matter - omega_lambda */
	omega_lambda_z, /* Omega_lambda at the given redshift */
	omega_matter_z,	/* Omega_matter at the given redshift */
	omhh,		/* Omega_matter * hubble^2 */
	onhh,		/* Omega_hdm * hubble^2 */
	p_c,		/* The correction to the exponent before drag epoch */
	p_cb,		/* The correction to the exponent after drag epoch */
	sound_horizon_fit,  /* The sound horizon at the drag epoch */
	theta_cmb,	/* The temperature of the CMB, in units of 2.7 K */
	y_drag,		/* Ratio of z_equality to z_drag */
	z_drag,		/* Redshift of the drag epoch */
	z_equality;	/* Redshift of matter-radiation equality */
	
	/* The following are set in TFmdm_onek_mpc() */
	float	gamma_eff,	/* Effective \Gamma */
	growth_cb,	/* Growth factor for CDM+Baryon perturbations */
	growth_cbnu,	/* Growth factor for CDM+Baryon+Neutrino pert. */
	max_fs_correction,  /* Correction near maximal free streaming */
	qq,		/* Wavenumber rescaled by \Gamma */
	qq_eff,		/* Wavenumber rescaled by effective Gamma */
	qq_nu,		/* Wavenumber compared to maximal free streaming */
	tf_master,	/* Master TF */
	tf_sup,		/* Suppressed TF */
	y_freestream; 	/* The epoch of free-streaming for a given scale */
	
	/* Finally, TFmdm_onek_mpc() and TFmdm_onek_hmpc() give their answers as */
	float   tf_cb,		/* The transfer function for density-weighted
						 CDM + Baryon perturbations. */
	tf_cbnu;	/* The transfer function for density-weighted
				 CDM + Baryon + Massive Neutrino perturbations. */
	
	/* By default, these functions return tf_cb */
	
	/* ------------------------- TFmdm_set_cosm() ------------------------ */
	int TFmdm_set_cosm(float omega_matter, float omega_baryon, float omega_hdm,
					   int degen_hdm, float omega_lambda, float hubble, float redshift)
	/* This routine takes cosmological parameters and a redshift and sets up
	 all the internal scalar quantities needed to compute the transfer function. */
	/* INPUT: omega_matter -- Density of CDM, baryons, and massive neutrinos,
	 in units of the critical density. */
	/* 	  omega_baryon -- Density of baryons, in units of critical. */
	/* 	  omega_hdm    -- Density of massive neutrinos, in units of critical */
	/* 	  degen_hdm    -- (Int) Number of degenerate massive neutrino species */
	/*        omega_lambda -- Cosmological constant */
	/* 	  hubble       -- Hubble constant, in units of 100 km/s/Mpc */
	/*        redshift     -- The redshift at which to evaluate */
	/* OUTPUT: Returns 0 if all is well, 1 if a warning was issued.  Otherwise,
	 sets many global variables for use in TFmdm_onek_mpc() */
	{
		float z_drag_b1, z_drag_b2, omega_denom;
		int qwarn;
		qwarn = 0;
		
		theta_cmb = 2.728/2.7;	/* Assuming T_cmb = 2.728 K */
		
		/* Look for strange input */
		if (omega_baryon<0.0) {
			fprintf(stderr,
					"TFmdm_set_cosm(): Negative omega_baryon set to trace amount.\n");
			qwarn = 1;
		}
		if (omega_hdm<0.0) {
			fprintf(stderr,
					"TFmdm_set_cosm(): Negative omega_hdm set to trace amount.\n");
			qwarn = 1;
		}
		if (hubble<=0.0) {
			fprintf(stderr,"TFmdm_set_cosm(): Negative Hubble constant illegal.\n");
			exit(1);  /* Can't recover */
		} else if (hubble>2.0) {
			fprintf(stderr,"TFmdm_set_cosm(): Hubble constant should be in units of 100 km/s/Mpc.\n");
			qwarn = 1;
		}
		if (redshift<=-1.0) {
			fprintf(stderr,"TFmdm_set_cosm(): Redshift < -1 is illegal.\n");
			exit(1);
		} else if (redshift>99.0) {
			fprintf(stderr,
					"TFmdm_set_cosm(): Large redshift entered.  TF may be inaccurate.\n");
			qwarn = 1;
		}
		if (degen_hdm<1) degen_hdm=1;
		num_degen_hdm = (float) degen_hdm;	
		/* Have to save this for TFmdm_onek_mpc() */
		/* This routine would crash if baryons or neutrinos were zero, 
		 so don't allow that */
		if (omega_baryon<=0) omega_baryon=1e-5;
		if (omega_hdm<=0) omega_hdm=1e-5;
		
		omega_curv = 1.0-omega_matter-omega_lambda;
		omhh = omega_matter*SQR(hubble);
		obhh = omega_baryon*SQR(hubble);
		onhh = omega_hdm*SQR(hubble);
		f_baryon = omega_baryon/omega_matter;
		f_hdm = omega_hdm/omega_matter;
		f_cdm = 1.0-f_baryon-f_hdm;
		f_cb = f_cdm+f_baryon;
		f_bnu = f_baryon+f_hdm;
		
		/* Compute the equality scale. */
		z_equality = 25000.0*omhh/SQR(SQR(theta_cmb));	/* Actually 1+z_eq */
		k_equality = 0.0746*omhh/SQR(theta_cmb);
		
		/* Compute the drag epoch and sound horizon */
		z_drag_b1 = 0.313*pow(omhh,-0.419)*(1+0.607*pow(omhh,0.674));
		z_drag_b2 = 0.238*pow(omhh,0.223);
		z_drag = 1291*pow(omhh,0.251)/(1.0+0.659*pow(omhh,0.828))*
		(1.0+z_drag_b1*pow(obhh,z_drag_b2));
		y_drag = z_equality/(1.0+z_drag);
		
		sound_horizon_fit = 44.5*log(9.83/omhh)/sqrt(1.0+10.0*pow(obhh,0.75));
		
		/* Set up for the free-streaming & infall growth function */
		p_c = 0.25*(5.0-sqrt(1+24.0*f_cdm));
		p_cb = 0.25*(5.0-sqrt(1+24.0*f_cb));
		
		omega_denom = omega_lambda+SQR(1.0+redshift)*(omega_curv+
													  omega_matter*(1.0+redshift));
		omega_lambda_z = omega_lambda/omega_denom;
		omega_matter_z = omega_matter*SQR(1.0+redshift)*(1.0+redshift)/omega_denom;
		growth_k0 = z_equality/(1.0+redshift)*2.5*omega_matter_z/
	    (pow(omega_matter_z,4.0/7.0)-omega_lambda_z+
		 (1.0+omega_matter_z/2.0)*(1.0+omega_lambda_z/70.0));
		growth_to_z0 = z_equality*2.5*omega_matter/(pow(omega_matter,4.0/7.0)
													-omega_lambda + (1.0+omega_matter/2.0)*(1.0+omega_lambda/70.0));
		growth_to_z0 = growth_k0/growth_to_z0;	
		
		/* Compute small-scale suppression */
		alpha_nu = f_cdm/f_cb*(5.0-2.*(p_c+p_cb))/(5.-4.*p_cb)*
		pow(1+y_drag,p_cb-p_c)*
		(1+f_bnu*(-0.553+0.126*f_bnu*f_bnu))/
		(1-0.193*sqrt(f_hdm*num_degen_hdm)+0.169*f_hdm*pow(num_degen_hdm,0.2))*
		(1+(p_c-p_cb)/2*(1+1/(3.-4.*p_c)/(7.-4.*p_cb))/(1+y_drag));
		alpha_gamma = sqrt(alpha_nu);
		beta_c = 1/(1-0.949*f_bnu);
		/* Done setting scalar variables */
		hhubble = hubble;	/* Need to pass Hubble constant to TFmdm_onek_hmpc() */
		return qwarn;
	}
	
	/* ---------------------------- TFmdm_onek_mpc() ---------------------- */
	
	float TFmdm_onek_mpc(float kk)
	/* Given a wavenumber in Mpc^-1, return the transfer function for the
	 cosmology held in the global variables. */
	/* Input: kk -- Wavenumber in Mpc^-1 */
	/* Output: The following are set as global variables:
	 growth_cb -- the transfer function for density-weighted
	 CDM + Baryon perturbations. 
	 growth_cbnu -- the transfer function for density-weighted
	 CDM + Baryon + Massive Neutrino perturbations. */
	/* The function returns growth_cb */
	{
		float tf_sup_L, tf_sup_C;
		float temp1, temp2;
		
		qq = kk/omhh*SQR(theta_cmb);
		
		/* Compute the scale-dependent growth functions */
		y_freestream = 17.2*f_hdm*(1+0.488*pow(f_hdm,-7.0/6.0))*
		SQR(num_degen_hdm*qq/f_hdm);
		temp1 = pow(growth_k0, 1.0-p_cb);
		temp2 = pow(growth_k0/(1+y_freestream),0.7);
		growth_cb = pow(1.0+temp2, p_cb/0.7)*temp1;
		growth_cbnu = pow(pow(f_cb,0.7/p_cb)+temp2, p_cb/0.7)*temp1;
		
		/* Compute the master function */
		gamma_eff =omhh*(alpha_gamma+(1-alpha_gamma)/
						 (1+SQR(SQR(kk*sound_horizon_fit*0.43))));
		qq_eff = qq*omhh/gamma_eff;
		
		tf_sup_L = log(2.71828+1.84*beta_c*alpha_gamma*qq_eff);
		tf_sup_C = 14.4+325/(1+60.5*pow(qq_eff,1.11));
		tf_sup = tf_sup_L/(tf_sup_L+tf_sup_C*SQR(qq_eff));
		
		qq_nu = 3.92*qq*sqrt(num_degen_hdm/f_hdm);
		max_fs_correction = 1+1.2*pow(f_hdm,0.64)*pow(num_degen_hdm,0.3+0.6*f_hdm)/
		(pow(qq_nu,-1.6)+pow(qq_nu,0.8));
		tf_master = tf_sup*max_fs_correction;
		
		/* Now compute the CDM+HDM+baryon transfer functions */
		tf_cb = tf_master*growth_cb/growth_k0;
		tf_cbnu = tf_master*growth_cbnu/growth_k0;
		return tf_cb;
	}
	
	/* ---------------------------- TFmdm_onek_hmpc() ---------------------- */
	
	float TFmdm_onek_hmpc(float kk)
	/* Given a wavenumber in h Mpc^-1, return the transfer function for the
	 cosmology held in the global variables. */
	/* Input: kk -- Wavenumber in h Mpc^-1 */
	/* Output: The following are set as global variables:
	 growth_cb -- the transfer function for density-weighted
	 CDM + Baryon perturbations. 
	 growth_cbnu -- the transfer function for density-weighted
	 CDM + Baryon + Massive Neutrino perturbations. */
	/* The function returns growth_cb */
	{
		return TFmdm_onek_mpc(kk*hhubble);
	}
	
	
	
	TransferFunction_EisensteinNeutrino( Cosmology aCosm, double Omega_HDM, int degen_HDM, double Tcmb = 2.726 )
    :  TransferFunction(aCosm)//, m_h0( aCosm.H0*0.01 )
	{
		TFmdm_set_cosm( aCosm.Omega_m, aCosm.Omega_b, Omega_HDM, degen_HDM, aCosm.Omega_L, aCosm.H0, aCosm.astart );
		
	}
	
	//! Computes the transfer function for k in Mpc/h by calling TFfit_onek
	virtual inline double compute( double k ){
		return TFmdm_onek_hmpc( k );
		
		
		/*double tfb, tfcdm, fb, fc; //, tfull
		TFfit_onek( k*m_h0, &tfb, &tfcdm );
		
		fb = m_Cosmology.Omega_b/(m_Cosmology.Omega_m);
		fc = (m_Cosmology.Omega_m-m_Cosmology.Omega_b)/(m_Cosmology.Omega_m) ;
		
		return fb*tfb+fc*tfcdm;*/
		//return 1.0;
	}
	
	
};

#endif
