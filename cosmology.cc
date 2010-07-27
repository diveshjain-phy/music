/*
 
 cosmology.cc - This file is part of MUSIC -
 a code to generate multi-scale initial conditions 
 for cosmological simulations 
 
 Copyright (C) 2010  Oliver Hahn
 
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

#include "cosmology.hh"
#include "mesh.hh"
#include "mg_operators.hh"

#define ACC(i,j,k) ((*u.get_grid((ilevel)))((i),(j),(k)))
#define SQR(x)	((x)*(x))


void compute_LLA_density( const grid_hierarchy& u, grid_hierarchy& fnew, unsigned order )
{
	fnew = u;
	
	for( unsigned ilevel=u.levelmin(); ilevel<=u.levelmax(); ++ilevel )
	{
		double h = pow(2.0,ilevel), h2 = h*h, h2_4 = 0.25*h2;
		meshvar_bnd *pvar = fnew.get_grid(ilevel);
		
		
		if( order == 2 )
		{
			#pragma omp parallel for //reduction(+:sum_corr,sum,sum2)
			for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
				for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
					for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
					{
						double D[3][3];
						
						D[0][0] = (ACC(ix-1,iy,iz)-2.0*ACC(ix,iy,iz)+ACC(ix+1,iy,iz)) * h2;
						D[1][1] = (ACC(ix,iy-1,iz)-2.0*ACC(ix,iy,iz)+ACC(ix,iy+1,iz)) * h2;
						D[2][2] = (ACC(ix,iy,iz-1)-2.0*ACC(ix,iy,iz)+ACC(ix,iy,iz+1)) * h2;
											
						D[0][1] = D[1][0] = (ACC(ix-1,iy-1,iz)-ACC(ix-1,iy+1,iz)-ACC(ix+1,iy-1,iz)+ACC(ix+1,iy+1,iz))*h2_4;
						D[0][2] = D[2][0] = (ACC(ix-1,iy,iz-1)-ACC(ix-1,iy,iz+1)-ACC(ix+1,iy,iz-1)+ACC(ix+1,iy,iz+1))*h2_4;
						D[1][2] = D[2][1] = (ACC(ix,iy-1,iz-1)-ACC(ix,iy-1,iz+1)-ACC(ix,iy+1,iz-1)+ACC(ix,iy+1,iz+1))*h2_4;
						
						(*pvar)(ix,iy,iz) = (D[0][0]+D[1][1]+D[2][2] -
							( D[0][0]*D[1][1] + D[0][0]*D[2][2] + D[1][1]*D[2][2] +
							  D[0][1]*D[1][0] + D[0][2]*D[2][0] + D[1][2]*D[2][1] +
							  D[0][0]*D[0][0] + D[1][1]*D[1][1] + D[2][2]*D[2][2] ));
						
					}
		}
		else if ( order == 4 )
		{
			#pragma omp parallel for 
			for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
				for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
					for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
					{
						double D[3][3];
						
						D[0][0] = (-ACC(ix-2,iy,iz)+16.*ACC(ix-1,iy,iz)-30.0*ACC(ix,iy,iz)+16.*ACC(ix+1,iy,iz)-ACC(ix+2,iy,iz)) * h2/12.0;
						D[1][1] = (-ACC(ix,iy-2,iz)+16.*ACC(ix,iy-1,iz)-30.0*ACC(ix,iy,iz)+16.*ACC(ix,iy+1,iz)-ACC(ix,iy+2,iz)) * h2/12.0;
						D[2][2] = (-ACC(ix,iy,iz-2)+16.*ACC(ix,iy,iz-1)-30.0*ACC(ix,iy,iz)+16.*ACC(ix,iy,iz+1)-ACC(ix,iy,iz+2)) * h2/12.0;
						
						D[0][1] = D[1][0] = (ACC(ix-1,iy-1,iz)-ACC(ix-1,iy+1,iz)-ACC(ix+1,iy-1,iz)+ACC(ix+1,iy+1,iz))*h2_4;
						D[0][2] = D[2][0] = (ACC(ix-1,iy,iz-1)-ACC(ix-1,iy,iz+1)-ACC(ix+1,iy,iz-1)+ACC(ix+1,iy,iz+1))*h2_4;
						D[1][2] = D[2][1] = (ACC(ix,iy-1,iz-1)-ACC(ix,iy-1,iz+1)-ACC(ix,iy+1,iz-1)+ACC(ix,iy+1,iz+1))*h2_4;
						
						(*pvar)(ix,iy,iz) = (D[0][0]+D[1][1]+D[2][2] -
						( D[0][0]*D[1][1] + D[0][0]*D[2][2] + D[1][1]*D[2][2] +
						 D[0][1]*D[1][0] + D[0][2]*D[2][0] + D[1][2]*D[2][1] +
						 D[0][0]*D[0][0] + D[1][1]*D[1][1] + D[2][2]*D[2][2] ));
						
					}
		}
		else if ( order == 6 )
		{
			h2_4/=36.;
			h2/=180.;
			#pragma omp parallel for 
			for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
				for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
					for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
					{
						double D[3][3];
						
						D[0][0] = (2.*ACC(ix-3,iy,iz)-27.*ACC(ix-2,iy,iz)+270.*ACC(ix-1,iy,iz)-490.0*ACC(ix,iy,iz)+270.*ACC(ix+1,iy,iz)-27.*ACC(ix+2,iy,iz)+2.*ACC(ix+3,iy,iz)) * h2;
						D[1][1] = (2.*ACC(ix,iy-3,iz)-27.*ACC(ix,iy-2,iz)+270.*ACC(ix,iy-1,iz)-490.0*ACC(ix,iy,iz)+270.*ACC(ix,iy+1,iz)-27.*ACC(ix,iy+2,iz)+2.*ACC(ix,iy+3,iz)) * h2;
						D[2][2] = (2.*ACC(ix,iy,iz-3)-27.*ACC(ix,iy,iz-2)+270.*ACC(ix,iy,iz-1)-490.0*ACC(ix,iy,iz)+270.*ACC(ix,iy,iz+1)-27.*ACC(ix,iy,iz+2)+2.*ACC(ix,iy,iz+3)) * h2;
						
						//.. this is actually 8th order accurate
						D[0][1] = D[1][0] = (64.*(ACC(ix-1,iy-1,iz)-ACC(ix-1,iy+1,iz)-ACC(ix+1,iy-1,iz)+ACC(ix+1,iy+1,iz))
											 -8.*(ACC(ix-2,iy-1,iz)-ACC(ix+2,iy-1,iz)-ACC(ix-2,iy+1,iz)+ACC(ix+2,iy+1,iz)
												+ ACC(ix-1,iy-2,iz)-ACC(ix-1,iy+2,iz)-ACC(ix+1,iy-2,iz)+ACC(ix+1,iy+2,iz))
											 +1.*(ACC(ix-2,iy-2,iz)-ACC(ix-2,iy+2,iz)-ACC(ix+2,iy-2,iz)+ACC(ix+2,iy+2,iz)))*h2_4;
						D[0][2] = D[2][0] = (64.*(ACC(ix-1,iy,iz-1)-ACC(ix-1,iy,iz+1)-ACC(ix+1,iy,iz-1)+ACC(ix+1,iy,iz+1))
											 -8.*(ACC(ix-2,iy,iz-1)-ACC(ix+2,iy,iz-1)-ACC(ix-2,iy,iz+1)+ACC(ix+2,iy,iz+1)
												+ ACC(ix-1,iy,iz-2)-ACC(ix-1,iy,iz+2)-ACC(ix+1,iy,iz-2)+ACC(ix+1,iy,iz+2))
											 +1.*(ACC(ix-2,iy,iz-2)-ACC(ix-2,iy,iz+2)-ACC(ix+2,iy,iz-2)+ACC(ix+2,iy,iz+2)))*h2_4;
						D[1][2] = D[2][1] = (64.*(ACC(ix,iy-1,iz-1)-ACC(ix,iy-1,iz+1)-ACC(ix,iy+1,iz-1)+ACC(ix,iy+1,iz+1))
											 -8.*(ACC(ix,iy-2,iz-1)-ACC(ix,iy+2,iz-1)-ACC(ix,iy-2,iz+1)+ACC(ix,iy+2,iz+1)
												+ ACC(ix,iy-1,iz-2)-ACC(ix,iy-1,iz+2)-ACC(ix,iy+1,iz-2)+ACC(ix,iy+1,iz+2))
											 +1.*(ACC(ix,iy-2,iz-2)-ACC(ix,iy-2,iz+2)-ACC(ix,iy+2,iz-2)+ACC(ix,iy+2,iz+2)))*h2_4;
						
						(*pvar)(ix,iy,iz) = (D[0][0]+D[1][1]+D[2][2] +
						( D[0][0]*D[1][1] + D[0][0]*D[2][2] + D[1][1]*D[2][2] +
						 D[0][1]*D[1][0] + D[0][2]*D[2][0] + D[1][2]*D[2][1] +
						 D[0][0]*D[0][0] + D[1][1]*D[1][1] + D[2][2]*D[2][2] ));
						
					}
			//TODO: test sixth order
			
		}else
			throw std::runtime_error("compute_LLA_density : invalid operator order specified");

	}
	
}


void compute_Lu_density( const grid_hierarchy& u, grid_hierarchy& fnew )
{
	fnew = u;
	
	for( unsigned ilevel=u.levelmin(); ilevel<=u.levelmax(); ++ilevel )
	{
		double h = pow(2.0,ilevel), h2 = h*h;
		meshvar_bnd *pvar = fnew.get_grid(ilevel);
		
		#pragma omp parallel for
		for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
			for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
				for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
				{
					double D[3][3];
					
					D[0][0] = 1.0 + (ACC(ix-1,iy,iz)-2.0*ACC(ix,iy,iz)+ACC(ix+1,iy,iz)) * h2;
					D[1][1] = 1.0 + (ACC(ix,iy-1,iz)-2.0*ACC(ix,iy,iz)+ACC(ix,iy+1,iz)) * h2;
					D[2][2] = 1.0 + (ACC(ix,iy,iz-1)-2.0*ACC(ix,iy,iz)+ACC(ix,iy,iz+1)) * h2;
					
					(*pvar)(ix,iy,iz) = -(D[0][0]+D[1][1]+D[2][2] - 3.0);
					
				}
	}
	
}

#pragma mark -

#ifdef SINGLE_PRECISION
#ifdef SINGLETHREAD_FFTW
#include <srfftw.h>
#else
#include <srfftw_threads.h>
#endif
#else
#ifdef SINGLETHREAD_FFTW
#include <drfftw.h>
#else
#include <drfftw_threads.h>
#endif
#endif

void compute_2LPT_source_FFT( config_file& cf_, const grid_hierarchy& u, grid_hierarchy& fnew )
{
	if( u.levelmin() != u.levelmax() )
		throw std::runtime_error("FFT 2LPT can only be run in Unigrid mode!");
	
	fnew = u;
	int nx,ny,nz,nzp;
	nx = u.get_grid(u.levelmax())->size(0);
	ny = u.get_grid(u.levelmax())->size(1);
	nz = u.get_grid(u.levelmax())->size(2);
	nzp = 2*(nz/2+1);
	
	//... copy data ..................................................
	fftw_real *data = new fftw_real[nx*ny*nzp];
	fftw_complex *cdata = reinterpret_cast<fftw_complex*> (data);
	
	fftw_complex	*cdata_11, *cdata_12, *cdata_13, *cdata_22, *cdata_23, *cdata_33;
	fftw_real		*data_11, *data_12, *data_13, *data_22, *data_23, *data_33;
	
	data_11 = new fftw_real[nx*ny*nzp]; cdata_11 = reinterpret_cast<fftw_complex*> (data_11);
	data_12 = new fftw_real[nx*ny*nzp]; cdata_12 = reinterpret_cast<fftw_complex*> (data_12);
	data_13 = new fftw_real[nx*ny*nzp]; cdata_13 = reinterpret_cast<fftw_complex*> (data_13);
	data_22 = new fftw_real[nx*ny*nzp]; cdata_22 = reinterpret_cast<fftw_complex*> (data_22);
	data_23 = new fftw_real[nx*ny*nzp]; cdata_23 = reinterpret_cast<fftw_complex*> (data_23);
	data_33 = new fftw_real[nx*ny*nzp]; cdata_33 = reinterpret_cast<fftw_complex*> (data_33);
	
	
	for( int i=0; i<nx; ++i )
		for( int j=0; j<ny; ++j )	
			for( int k=0; k<nz; ++k )
			{
				unsigned idx = (i*ny+j)*nzp+k;
				data[idx] = (*u.get_grid(u.levelmax()))(i,j,k);
			}
	
	//... perform FFT and Poisson solve................................
	rfftwnd_plan 
	plan = rfftw3d_create_plan( nx,ny,nz,
							   FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE|FFTW_IN_PLACE),
	iplan = rfftw3d_create_plan( nx,ny,nz,
								FFTW_COMPLEX_TO_REAL, FFTW_ESTIMATE|FFTW_IN_PLACE);
	
	
#ifndef SINGLETHREAD_FFTW		
	rfftwnd_threads_one_real_to_complex( omp_get_max_threads(), plan, data, NULL );
#else
	rfftwnd_one_real_to_complex( plan, data, NULL );
#endif
	
	//double fac = -1.0/(nx*ny*nz);
	double boxlength = cf_.getValue<double>("setup","boxlength");
	double kfac = 2.0*M_PI;///boxlength;
	double norm = 1.0/((double)(nx*ny*nz));///sqrt((double)(nx*ny*nz));
	
	
	for( int i=0; i<nx; ++i )
		for( int j=0; j<ny; ++j )	
			for( int l=0; l<nz/2+1; ++l )
			{
				int ii = i; if(ii>nx/2) ii-=nx;
				int jj = j; if(jj>ny/2) jj-=ny;
				double ki = (double)ii;
				double kj = (double)jj;
				double kk = (double)l;
				
				double k[3];
				k[0] = (double)ki * kfac;
				k[1] = (double)kj * kfac;
				k[2] = (double)kk * kfac;
				
				unsigned idx = (i*ny+j)*nzp/2+l;
				//double re = cdata[idx].re;
				//double im = cdata[idx].im;
				
				cdata_11[idx].re = -k[0]*k[0] * cdata[idx].re * norm;
				cdata_11[idx].im = -k[0]*k[0] * cdata[idx].im * norm;
				
				cdata_12[idx].re = -k[0]*k[1] * cdata[idx].re * norm;
				cdata_12[idx].im = -k[0]*k[1] * cdata[idx].im * norm;
				
				cdata_13[idx].re = -k[0]*k[2] * cdata[idx].re * norm;
				cdata_13[idx].im = -k[0]*k[2] * cdata[idx].im * norm;
				
				cdata_22[idx].re = -k[1]*k[1] * cdata[idx].re * norm;
				cdata_22[idx].im = -k[1]*k[1] * cdata[idx].im * norm;
				
				cdata_23[idx].re = -k[1]*k[2] * cdata[idx].re * norm;
				cdata_23[idx].im = -k[1]*k[2] * cdata[idx].im * norm;
				
				cdata_33[idx].re = -k[2]*k[2] * cdata[idx].re * norm;
				cdata_33[idx].im = -k[2]*k[2] * cdata[idx].im * norm;
				
			}
	
	delete[] data;
	cdata_11[0].re	= 0.0; cdata_11[0].im	= 0.0;
	cdata_12[0].re	= 0.0; cdata_12[0].im	= 0.0;
	cdata_13[0].re	= 0.0; cdata_13[0].im	= 0.0;
	cdata_22[0].re	= 0.0; cdata_22[0].im	= 0.0;
	cdata_23[0].re	= 0.0; cdata_23[0].im	= 0.0;
	cdata_33[0].re	= 0.0; cdata_33[0].im	= 0.0;
	
	
#ifndef SINGLETHREAD_FFTW		
	//rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata, NULL );
	rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata_11, NULL );
	rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata_12, NULL );
	rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata_13, NULL );
	rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata_22, NULL );
	rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata_23, NULL );
	rfftwnd_threads_one_complex_to_real( omp_get_max_threads(), iplan, cdata_33, NULL );
#else
	//rfftwnd_one_complex_to_real( iplan, cdata, NULL );
	rfftwnd_one_complex_to_real(iplan, cdata_11, NULL );
	rfftwnd_one_complex_to_real(iplan, cdata_12, NULL );
	rfftwnd_one_complex_to_real(iplan, cdata_13, NULL );
	rfftwnd_one_complex_to_real(iplan, cdata_22, NULL );
	rfftwnd_one_complex_to_real(iplan, cdata_23, NULL );
	rfftwnd_one_complex_to_real(iplan, cdata_33, NULL );
#endif
	
	
	
	rfftwnd_destroy_plan(plan);
	rfftwnd_destroy_plan(iplan);
	

	//... copy data ..........................................
	for( int i=0; i<nx; ++i )
		for( int j=0; j<ny; ++j )	
			for( int k=0; k<nz; ++k )
			{
				unsigned ii = (i*ny+j)*nzp+k;
				(*fnew.get_grid(u.levelmax()))(i,j,k) = -(( data_11[ii]*data_22[ii]-data_12[ii]*data_12[ii] ) +
														 ( data_11[ii]*data_33[ii]-data_13[ii]*data_13[ii] ) +
														 ( data_22[ii]*data_33[ii]-data_23[ii]*data_23[ii] ) );
			}
	
	//delete[] data;
	delete[] data_11;
	delete[] data_12;
	delete[] data_13;
	delete[] data_23;
	delete[] data_22;
	delete[] data_33;
}


void compute_2LPT_source( const grid_hierarchy& u, grid_hierarchy& fnew, unsigned order )
{
	fnew = u;
	
	for( unsigned ilevel=u.levelmin(); ilevel<=u.levelmax(); ++ilevel )
	{
		double h = pow(2.0,ilevel), h2 = h*h, h2_4 = 0.25*h2;
		meshvar_bnd *pvar = fnew.get_grid(ilevel);
		
		if ( order == 2 )
		{
			
			#pragma omp parallel for
			for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
				for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
					for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
					{
						double D[3][3];
						
						D[0][0] = (ACC(ix-1,iy,iz)-2.0*ACC(ix,iy,iz)+ACC(ix+1,iy,iz)) * h2;
						D[1][1] = (ACC(ix,iy-1,iz)-2.0*ACC(ix,iy,iz)+ACC(ix,iy+1,iz)) * h2;
						D[2][2] = (ACC(ix,iy,iz-1)-2.0*ACC(ix,iy,iz)+ACC(ix,iy,iz+1)) * h2;
						D[0][1] = D[1][0] = (ACC(ix-1,iy-1,iz)-ACC(ix-1,iy+1,iz)-ACC(ix+1,iy-1,iz)+ACC(ix+1,iy+1,iz))*h2_4;
						D[0][2] = D[2][0] = (ACC(ix-1,iy,iz-1)-ACC(ix-1,iy,iz+1)-ACC(ix+1,iy,iz-1)+ACC(ix+1,iy,iz+1))*h2_4;
						D[1][2] = D[2][1] = (ACC(ix,iy-1,iz-1)-ACC(ix,iy-1,iz+1)-ACC(ix,iy+1,iz-1)+ACC(ix,iy+1,iz+1))*h2_4;
						
						
						(*pvar)(ix,iy,iz) =  -( D[0][0]*D[1][1] - SQR( D[0][1] )
											+ D[0][0]*D[2][2] - SQR( D[0][2] )
											+ D[1][1]*D[2][2] - SQR( D[1][2] ));
						
					}
		}
		else if ( order == 4 )
		{
			#pragma omp parallel for
			for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
				for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
					for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
					{
						double D[3][3];
						
						D[0][0] = (-ACC(ix-2,iy,iz)+16.*ACC(ix-1,iy,iz)-30.0*ACC(ix,iy,iz)+16.*ACC(ix+1,iy,iz)-ACC(ix+2,iy,iz)) * h2/12.0;
						D[1][1] = (-ACC(ix,iy-2,iz)+16.*ACC(ix,iy-1,iz)-30.0*ACC(ix,iy,iz)+16.*ACC(ix,iy+1,iz)-ACC(ix,iy+2,iz)) * h2/12.0;
						D[2][2] = (-ACC(ix,iy,iz-2)+16.*ACC(ix,iy,iz-1)-30.0*ACC(ix,iy,iz)+16.*ACC(ix,iy,iz+1)-ACC(ix,iy,iz+2)) * h2/12.0;
						D[0][1] = D[1][0] = (ACC(ix-1,iy-1,iz)-ACC(ix-1,iy+1,iz)-ACC(ix+1,iy-1,iz)+ACC(ix+1,iy+1,iz))*h2_4;
						D[0][2] = D[2][0] = (ACC(ix-1,iy,iz-1)-ACC(ix-1,iy,iz+1)-ACC(ix+1,iy,iz-1)+ACC(ix+1,iy,iz+1))*h2_4;
						D[1][2] = D[2][1] = (ACC(ix,iy-1,iz-1)-ACC(ix,iy-1,iz+1)-ACC(ix,iy+1,iz-1)+ACC(ix,iy+1,iz+1))*h2_4;
						
						
						(*pvar)(ix,iy,iz) =  -( D[0][0]*D[1][1] - SQR( D[0][1] )
						+ D[0][0]*D[2][2] - SQR( D[0][2] )
						+ D[1][1]*D[2][2] - SQR( D[1][2] ));
						
					}
		}
		else if ( order == 6 )
		{
			h2_4/=36.;
			h2/=180.;
#pragma omp parallel for 
			for( int ix = 0; ix < (int)(*u.get_grid(ilevel)).size(0); ++ix )
				for( int iy = 0; iy < (int)(*u.get_grid(ilevel)).size(1); ++iy )
					for( int iz = 0; iz < (int)(*u.get_grid(ilevel)).size(2); ++iz )
					{
						double D[3][3];
						
						D[0][0] = (2.*ACC(ix-3,iy,iz)-27.*ACC(ix-2,iy,iz)+270.*ACC(ix-1,iy,iz)-490.0*ACC(ix,iy,iz)+270.*ACC(ix+1,iy,iz)-27.*ACC(ix+2,iy,iz)+2.*ACC(ix+3,iy,iz)) * h2;
						D[1][1] = (2.*ACC(ix,iy-3,iz)-27.*ACC(ix,iy-2,iz)+270.*ACC(ix,iy-1,iz)-490.0*ACC(ix,iy,iz)+270.*ACC(ix,iy+1,iz)-27.*ACC(ix,iy+2,iz)+2.*ACC(ix,iy+3,iz)) * h2;
						D[2][2] = (2.*ACC(ix,iy,iz-3)-27.*ACC(ix,iy,iz-2)+270.*ACC(ix,iy,iz-1)-490.0*ACC(ix,iy,iz)+270.*ACC(ix,iy,iz+1)-27.*ACC(ix,iy,iz+2)+2.*ACC(ix,iy,iz+3)) * h2;
						
						//.. this is actually 8th order accurate
						D[0][1] = D[1][0] = (64.*(ACC(ix-1,iy-1,iz)-ACC(ix-1,iy+1,iz)-ACC(ix+1,iy-1,iz)+ACC(ix+1,iy+1,iz))
											 -8.*(ACC(ix-2,iy-1,iz)-ACC(ix+2,iy-1,iz)-ACC(ix-2,iy+1,iz)+ACC(ix+2,iy+1,iz)
												  + ACC(ix-1,iy-2,iz)-ACC(ix-1,iy+2,iz)-ACC(ix+1,iy-2,iz)+ACC(ix+1,iy+2,iz))
											 +1.*(ACC(ix-2,iy-2,iz)-ACC(ix-2,iy+2,iz)-ACC(ix+2,iy-2,iz)+ACC(ix+2,iy+2,iz)))*h2_4;
						D[0][2] = D[2][0] = (64.*(ACC(ix-1,iy,iz-1)-ACC(ix-1,iy,iz+1)-ACC(ix+1,iy,iz-1)+ACC(ix+1,iy,iz+1))
											 -8.*(ACC(ix-2,iy,iz-1)-ACC(ix+2,iy,iz-1)-ACC(ix-2,iy,iz+1)+ACC(ix+2,iy,iz+1)
												  + ACC(ix-1,iy,iz-2)-ACC(ix-1,iy,iz+2)-ACC(ix+1,iy,iz-2)+ACC(ix+1,iy,iz+2))
											 +1.*(ACC(ix-2,iy,iz-2)-ACC(ix-2,iy,iz+2)-ACC(ix+2,iy,iz-2)+ACC(ix+2,iy,iz+2)))*h2_4;
						D[1][2] = D[2][1] = (64.*(ACC(ix,iy-1,iz-1)-ACC(ix,iy-1,iz+1)-ACC(ix,iy+1,iz-1)+ACC(ix,iy+1,iz+1))
											 -8.*(ACC(ix,iy-2,iz-1)-ACC(ix,iy+2,iz-1)-ACC(ix,iy-2,iz+1)+ACC(ix,iy+2,iz+1)
												  + ACC(ix,iy-1,iz-2)-ACC(ix,iy-1,iz+2)-ACC(ix,iy+1,iz-2)+ACC(ix,iy+1,iz+2))
											 +1.*(ACC(ix,iy-2,iz-2)-ACC(ix,iy-2,iz+2)-ACC(ix,iy+2,iz-2)+ACC(ix,iy+2,iz+2)))*h2_4;
						
						(*pvar)(ix,iy,iz) =  -( D[0][0]*D[1][1] - SQR( D[0][1] )
											+ D[0][0]*D[2][2] - SQR( D[0][2] )
											+ D[1][1]*D[2][2] - SQR( D[1][2] ) );
						
					}
			//TODO: test sixth order
			
		}
		else
			throw std::runtime_error("compute_2LPT_source : invalid operator order specified");


	}
	
	
	//.. subtract global mean so the multi-grid poisson solver behaves well
	
	for( int i=fnew.levelmax(); i>(int)fnew.levelmin(); --i )
		mg_straight().restrict( (*fnew.get_grid(i)), (*fnew.get_grid(i-1)) );
	
	double sum = 0.0;
	int nx,ny,nz;
	
	nx = fnew.get_grid(fnew.levelmin())->size(0);
	ny = fnew.get_grid(fnew.levelmin())->size(1);
	nz = fnew.get_grid(fnew.levelmin())->size(2);
	
	for( int ix=0; ix<nx; ++ix )
		for( int iy=0; iy<ny; ++iy )
			for( int iz=0; iz<nz; ++iz )
				sum += (*fnew.get_grid(fnew.levelmin()))(ix,iy,iz);
	
	sum /= (nx*ny*nz);
	
	for( unsigned i=fnew.levelmin(); i<=fnew.levelmax(); ++i )
	{		
		nx = fnew.get_grid(i)->size(0);
		ny = fnew.get_grid(i)->size(1);
		nz = fnew.get_grid(i)->size(2);
		
		for( int ix=0; ix<nx; ++ix )
			for( int iy=0; iy<ny; ++iy )
				for( int iz=0; iz<nz; ++iz )
					(*fnew.get_grid(i))(ix,iy,iz) -= sum;
	}
	
}
#undef SQR
#undef ACC

