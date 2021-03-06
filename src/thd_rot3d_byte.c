/*****************************************************************************
   Major portions of this software are copyrighted by the Medical College
   of Wisconsin, 1994-2000, and are released under the Gnu General Public
   License, Version 2.  See the file README.Copyright for details.
******************************************************************************/
   

/*===========================================================================
  Routines to rotate/shift a 3D volume of bytes using a 4 way shear
  decomposition and "two-step" interpolation -- RWCox - Oct 2000.
=============================================================================*/

#include "thd_shear3d.h"

#define CACHE 7168 /* good for Pentium processors */

#define TSBOT 0.3  /* the "optimal" breakpoints for ts_shift */
#define TSTOP 0.7

#define NNBOT 0.5

/*---------------------------------------------------------------------------*/

static int icode  = MRI_TSSHIFT ;
static float sbot = TSBOT ;

void THD_rota_byte_mode( int code )
{
   if( code == MRI_NN ){
      icode = MRI_NN      ; sbot = NNBOT ;
   } else {
      icode = MRI_TSSHIFT ; sbot = TSBOT ;
   }
}

/*---------------------------------------------------------------------------*/

#undef  DTYPE
#define DTYPE byte  /* AKA unsigned char */
#undef  DSIZE
#define DSIZE 1     /* sizeof(DTYPE) */

static int    nlcbuf = 0 ;     /* workspace */
static DTYPE * lcbuf = NULL ;

/*---------------------------------------------------------------------------*/

static int nn_shift_byte( int n , float af , DTYPE * f )
{
   register int ii , ia ;
   float aa ;
   int ibot,itop ;

   if( fabs(af) < NNBOT ) return 0 ; /* do nothing if shift is too small */

   for( ii=0 ; ii < n && f[ii] == 0 ; ii++ ) ; /* nada */
   if( ii == n ) return 0 ;          /* do nothing if data all zero */

   af = -af ; ia = (int) af ; if( af < 0 ) ia-- ;  /* ia = floor */
   aa = af - ia ;

   if( n > nlcbuf ){
      if( lcbuf != NULL ) free(lcbuf) ;
      lcbuf  = (DTYPE *) malloc( DSIZE * n ) ;
      nlcbuf = n ;
   }

   ibot = -ia  ;   if( ibot < 0   ) ibot = 0 ;
   itop = n-2-ia ; if( itop > n-1 ) itop = n-1 ;

#if 1
   memset(lcbuf,0,n*DSIZE) ;   /* seems to be faster */
#else
   memset(lcbuf,0,ibot*DSIZE) ;
   memset(lcbuf+(itop+1),0,(n-(itop+1))*DSIZE) ;
#endif

   if( aa < NNBOT ){         /* NN to bottom */

      memcpy( lcbuf+ibot, f+(ibot+ia)  , (itop+1-ibot)*DSIZE ) ;

   } else {                  /* NN to top */

      memcpy( lcbuf+ibot, f+(ibot+1+ia), (itop+1-ibot)*DSIZE ) ;

   }
   memcpy( f , lcbuf , DSIZE*n ) ;
   return 1 ;
}

/*---------------------------------------------------------------------------
   Two-step interpolation and shifting
-----------------------------------------------------------------------------*/

static int ts_shift_byte( int n , float af , DTYPE * f )
{
   register int ii , ia , ix ;
   float aa ;
   int ibot,itop ;

   if( fabs(af) < TSBOT ) return 0 ; /* do nothing if shift is too small */

   for( ii=0 ; ii < n && f[ii] == 0 ; ii++ ) ; /* nada */
   if( ii == n ) return 0 ;          /* do nothing if data all zero */

   af = -af ; ia = (int) af ; if( af < 0 ) ia-- ;  /* ia = floor */
   aa = af - ia ;

   if( n > nlcbuf ){
      if( lcbuf != NULL ) free(lcbuf) ;
      lcbuf  = (DTYPE *) malloc( DSIZE * n ) ;
      nlcbuf = n ;
   }

   ibot = -ia  ;   if( ibot < 0   ) ibot = 0 ;
   itop = n-2-ia ; if( itop > n-1 ) itop = n-1 ;

#if 1
   memset(lcbuf,0,n*DSIZE) ;   /* seems to be faster */
#else
   memset(lcbuf,0,ibot*DSIZE) ;
   memset(lcbuf+(itop+1),0,(n-(itop+1))*DSIZE) ;
#endif

   if( aa < TSBOT ){         /* NN to bottom */

      memcpy( lcbuf+ibot, f+(ibot+ia)  , (itop+1-ibot)*DSIZE ) ;

   } else if( aa > TSTOP ){  /* NN to top */

      memcpy( lcbuf+ibot, f+(ibot+1+ia), (itop+1-ibot)*DSIZE ) ;

   } else {                  /* average bottom and top */

      for( ii=ibot ; ii <= itop ; ii++ ){
         ix = ii + ia ; lcbuf[ii] = ( f[ix] + f[ix+1] ) >> 1 ;
      }

   }
   memcpy( f , lcbuf , DSIZE*n ) ;
   return 1 ;
}

/*---------------------------------------------------------------------------
   Flip a 3D array about the (x,y) axes:
    i <--> nx-1-i    j <--> ny-1-j
-----------------------------------------------------------------------------*/

#define VV(i,j,k) v[(i)+(j)*nx+(k)*nxy]
#define SX(i)     (nx1-(i))
#define SY(j)     (ny1-(j))
#define SZ(k)     (nz1-(k))

static void flip_xy( int nx , int ny , int nz , DTYPE * v , Tmask * tm )
{
   int ii,jj,kk ;
   int nx1=nx-1,nx2=nx/2, ny1=ny-1,ny2=ny/2, nz1=nz-1,nz2=nz/2, nxy=nx*ny ;
   DTYPE * r1 ;

   r1 = (DTYPE *) malloc(DSIZE*nx) ;  /* save 1 row */

   for( kk=0 ; kk < nz ; kk++ ){              /* for each slice */
      for( jj=0 ; jj < ny2 ; jj++ ){          /* first 1/2 of rows */

         /* swap rows jj and ny1-jj, flipping them in ii as well */

         if( TM_XLINE(tm,jj+kk*ny) || TM_XLINE(tm,SY(jj)+kk*ny) ){
            for( ii=0; ii < nx; ii++ ) r1[ii]           = VV(SX(ii),SY(jj),kk) ;
            for( ii=0; ii < nx; ii++ ) VV(ii,SY(jj),kk) = VV(SX(ii),jj    ,kk) ;
            for( ii=0; ii < nx; ii++ ) VV(ii,jj    ,kk) = r1[ii] ;
         }
      }
      if( ny%2 == 1 && TM_XLINE(tm,jj+kk*ny) ){  /* central row? */
         for( ii=0; ii < nx; ii++ ) r1[ii]       = VV(SX(ii),jj,kk); /* flip it */
         for( ii=0; ii < nx; ii++ ) VV(ii,jj,kk) = r1[ii] ;          /* restore */
      }
   }

   free(r1) ; return ;
}

/*---------------------------------------------------------------------------
   Flip a 3D array about the (y,z) axes:
     j <--> ny-1-j   k <--> nz-1-k
-----------------------------------------------------------------------------*/

static void flip_yz( int nx , int ny , int nz , DTYPE * v , Tmask * tm )
{
   int ii,jj,kk ;
   int nx1=nx-1,nx2=nx/2, ny1=ny-1,ny2=ny/2, nz1=nz-1,nz2=nz/2, nxy=nx*ny ;
   DTYPE * r1 ;

   r1 = (DTYPE *) malloc(DSIZE*ny) ;

   for( ii=0 ; ii < nx ; ii++ ){
      for( kk=0 ; kk < nz2 ; kk++ ){
         if( TM_YLINE(tm,kk+ii*nz) || TM_YLINE(tm,SZ(kk)+ii*nz) ){
            for( jj=0; jj < ny; jj++ ) r1[jj]           = VV(ii,SY(jj),SZ(kk)) ;
            for( jj=0; jj < ny; jj++ ) VV(ii,jj,SZ(kk)) = VV(ii,SY(jj),kk    ) ;
            for( jj=0; jj < ny; jj++ ) VV(ii,jj,kk    ) = r1[jj] ;
         }
      }
      if( nz%2 == 1 && TM_YLINE(tm,kk+ii*nz) ){
         for( jj=0; jj < ny; jj++ ) r1[jj]       = VV(ii,SY(jj),kk) ;
         for( jj=0; jj < ny; jj++ ) VV(ii,jj,kk) = r1[jj] ;
      }
   }

   free(r1) ; return ;
}

/*---------------------------------------------------------------------------
   Flip a 3D array about the (x,z) axes:
     i <--> nx-1-i   k <--> nz-1-k
-----------------------------------------------------------------------------*/

static void flip_xz( int nx , int ny , int nz , DTYPE * v , Tmask * tm )
{
   int ii,jj,kk ;
   int nx1=nx-1,nx2=nx/2, ny1=ny-1,ny2=ny/2, nz1=nz-1,nz2=nz/2, nxy=nx*ny ;
   DTYPE * r1 ;

   r1 = (DTYPE *) malloc(DSIZE*nx) ;

   for( jj=0 ; jj < ny ; jj++ ){
      for( kk=0 ; kk < nz2 ; kk++ ){
         if( TM_XLINE(tm,jj+kk*ny) || TM_XLINE(tm,jj+SZ(kk)*ny) ){
            for( ii=0; ii < nx; ii++ ) r1[ii]           = VV(SX(ii),jj,SZ(kk)) ;
            for( ii=0; ii < nx; ii++ ) VV(ii,jj,SZ(kk)) = VV(SX(ii),jj,kk    ) ;
            for( ii=0; ii < nx; ii++ ) VV(ii,jj,kk    ) = r1[ii] ;
         }
      }
      if( nz%2 == 1 && TM_XLINE(tm,jj+kk*ny) ){
         for( ii=0; ii < nx; ii++ ) r1[ii]       = VV(SX(ii),jj,kk) ;
         for( ii=0; ii < nx; ii++ ) VV(ii,jj,kk) = r1[ii] ;
      }
   }

   free(r1) ; return ;
}

/*---------------------------------------------------------------------------
   Apply an x-axis shear to a 3D array: x -> x + a*y + b*z + s
   (dilation factor "f" assumed to be 1.0)
-----------------------------------------------------------------------------*/

static void apply_xshear( float a , float b , float s ,
                          int nx , int ny , int nz , DTYPE * v , Tmask * tm )
{
   DTYPE * fj0 ;
   int   nx1=nx-1    , ny1=ny-1    , nz1=nz-1    , nxy=nx*ny ;
   float nx2=0.5*nx1 , ny2=0.5*ny1 , nz2=0.5*nz1 ;
   int ii,jj,kk ;
   float st ;

   /* don't do anything if shift is too small */

   st = fabs(a)*ny2 + fabs(b)*nz2 + fabs(s); if( st < sbot ) return ;

   switch( icode ){
      default:
      case MRI_TSSHIFT:
        for( kk=0 ; kk < nz ; kk++ ){
          for( jj=0 ; jj < ny ; jj++ )
            if( TM_XLINE(tm,jj+kk*ny) )
              ts_shift_byte( nx, a*(jj-ny2)+b*(kk-nz2)+s, v+(jj*nx+kk*nxy) );
        }
      break ;

      case MRI_NN:
        for( kk=0 ; kk < nz ; kk++ ){
          for( jj=0 ; jj < ny ; jj++ )
            if( TM_XLINE(tm,jj+kk*ny) )
              nn_shift_byte( nx, a*(jj-ny2)+b*(kk-nz2)+s, v+(jj*nx+kk*nxy) );
        }
      break ;
   }

   return ;
}

/*---------------------------------------------------------------------------
   Apply a y-axis shear to a 3D array: y -> y + a*x + b*z + s
-----------------------------------------------------------------------------*/

static void apply_yshear( float a , float b , float s ,
                          int nx , int ny , int nz , DTYPE * v , Tmask * tm )
{
   DTYPE * fj0 ;
   int   nx1=nx-1    , ny1=ny-1    , nz1=nz-1    , nxy=nx*ny ;
   float nx2=0.5*nx1 , ny2=0.5*ny1 , nz2=0.5*nz1 ;
   int ii,jj,kk ;
   float st ;
   int xnum , xx , xtop , *wk ;

   /* don't do anything if shift is too small */

   st = fabs(a)*nx2 + fabs(b)*nz2 + fabs(s) ; if( st < sbot ) return ;

   xnum = CACHE / (ny*DSIZE) ; if( xnum < 1 ) xnum = 1 ;
   fj0 = (DTYPE *) malloc( DSIZE * xnum*ny ) ;
   wk  = (int *)   malloc( sizeof(int)*xnum ) ;

   switch( icode ){
      default:
      case MRI_TSSHIFT:
        for( kk=0 ; kk < nz ; kk++ ){
          for( ii=0 ; ii < nx ; ii+=xnum ){
            xtop = MIN(nx-ii,xnum) ;
            for( xx=0 ; xx < xtop ; xx++ )
              wk[xx] = fabs(a*(ii+xx-nx2)+b*(kk-nz2)+s) > TSBOT
                       && TM_YLINE(tm,kk+(ii+xx)*nz) ;
            for( jj=0; jj < ny; jj++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) fj0[jj+xx*ny] = VV(ii+xx,jj,kk) ;
            for( xx=0 ; xx < xtop ; xx++ )
              if( wk[xx] )
               wk[xx] = ts_shift_byte(ny, a*(ii+xx-nx2)+b*(kk-nz2)+s, fj0+xx*ny);
            for( jj=0; jj < ny; jj++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) VV(ii+xx,jj,kk) = fj0[jj+xx*ny] ;
          }
        }
      break ;

      case MRI_NN:
        for( kk=0 ; kk < nz ; kk++ ){
          for( ii=0 ; ii < nx ; ii+=xnum ){
            xtop = MIN(nx-ii,xnum) ;
            for( xx=0 ; xx < xtop ; xx++ )
              wk[xx] = fabs(a*(ii+xx-nx2)+b*(kk-nz2)+s) > NNBOT
                       && TM_YLINE(tm,kk+(ii+xx)*nz) ;
            for( jj=0; jj < ny; jj++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) fj0[jj+xx*ny] = VV(ii+xx,jj,kk) ;
            for( xx=0 ; xx < xtop ; xx++ )
              if( wk[xx] )
               wk[xx] = nn_shift_byte(ny, a*(ii+xx-nx2)+b*(kk-nz2)+s, fj0+xx*ny);
            for( jj=0; jj < ny; jj++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) VV(ii+xx,jj,kk) = fj0[jj+xx*ny] ;
          }
        }
      break ;
   }

   free(wk) ; free(fj0) ; return ;
}

/*---------------------------------------------------------------------------
   Apply a z-axis shear to a 3D array: z -> z + a*x + b*y + s
-----------------------------------------------------------------------------*/

static void apply_zshear( float a , float b , float s ,
                          int nx , int ny , int nz , DTYPE * v , Tmask * tm )
{
   DTYPE * fj0 ;
   int   nx1=nx-1    , ny1=ny-1    , nz1=nz-1    , nxy=nx*ny ;
   float nx2=0.5*nx1 , ny2=0.5*ny1 , nz2=0.5*nz1 ;
   int ii,jj,kk ;
   float st ;
   int xnum , xx , xtop , *wk ;

   /* don't do anything if shift is too small */

   st = fabs(a)*nx2 + fabs(b)*ny2 + fabs(s) ; if( st < sbot ) return ;

   xnum = CACHE / (nz*DSIZE) ; if( xnum < 1 ) xnum = 1 ;
   fj0 = (DTYPE *) malloc( DSIZE * xnum*nz ) ;
   wk  = (int *)   malloc( sizeof(int)*xnum ) ;

   switch( icode ){
      default:
      case MRI_TSSHIFT:
        for( jj=0 ; jj < ny ; jj++ ){
          for( ii=0 ; ii < nx ; ii+=xnum ){
            xtop = MIN(nx-ii,xnum) ;
            for( xx=0 ; xx < xtop ; xx++ )
               wk[xx] = fabs(a*(ii+xx-nx2)+b*(jj-ny2)+s) > TSBOT
                        && TM_ZLINE(tm,ii+jj*nx+xx) ;
            for( kk=0; kk < nz; kk++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) fj0[kk+xx*nz] = VV(ii+xx,jj,kk) ;
            for( xx=0 ; xx < xtop ; xx++ )
              if( wk[xx] )
               wk[xx] = ts_shift_byte(nz, a*(ii+xx-nx2)+b*(jj-ny2)+s, fj0+xx*nz);
            for( kk=0; kk < nz; kk++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) VV(ii+xx,jj,kk) = fj0[kk+xx*nz] ;
          }
        }
      break ;

      case MRI_NN:
        for( jj=0 ; jj < ny ; jj++ ){
          for( ii=0 ; ii < nx ; ii+=xnum ){
            xtop = MIN(nx-ii,xnum) ;
            for( xx=0 ; xx < xtop ; xx++ )
               wk[xx] = fabs(a*(ii+xx-nx2)+b*(jj-ny2)+s) > NNBOT
                        && TM_ZLINE(tm,ii+jj*nx+xx) ;
            for( kk=0; kk < nz; kk++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) fj0[kk+xx*nz] = VV(ii+xx,jj,kk) ;
            for( xx=0 ; xx < xtop ; xx++ )
              if( wk[xx] )
               wk[xx] = nn_shift_byte(nz, a*(ii+xx-nx2)+b*(jj-ny2)+s, fj0+xx*nz);
            for( kk=0; kk < nz; kk++ )
              for( xx=0 ; xx < xtop ; xx++ )
                if( wk[xx] ) VV(ii+xx,jj,kk) = fj0[kk+xx*nz] ;
          }
        }
      break ;
   }

   free(wk) ; free(fj0) ; return ;
}

/*---------------------------------------------------------------------------
   Apply a set of shears to a 3D array of bytes.
   Note that we assume that the dilation factors ("f") are all 1.
-----------------------------------------------------------------------------*/

static void apply_3shear( MCW_3shear shr ,
                          int nx, int ny, int nz, DTYPE * vol , Tmask * tm )
{
   int qq ;
   float a , b , s ;

   if( ! ISVALID_3SHEAR(shr) ) return ;

   /* carry out a preliminary 180 flippo ? */

   if( shr.flip0 >= 0 ){
      switch( shr.flip0 + shr.flip1 ){
         case 1: flip_xy( nx,ny,nz,vol,tm ) ; break ;
         case 2: flip_xz( nx,ny,nz,vol,tm ) ; break ;
         case 3: flip_yz( nx,ny,nz,vol,tm ) ; break ;
      }
   }

   /* apply each shear */

   for( qq=0 ; qq < 4 ; qq++ ){
      switch( shr.ax[qq] ){
         case 0:
            a = shr.scl[qq][1] ;
            b = shr.scl[qq][2] ;
            s = shr.sft[qq]    ;
            apply_xshear( a,b,s , nx,ny,nz , vol , (qq==0)? tm : NULL ) ;
         break ;

         case 1:
            a = shr.scl[qq][0] ;
            b = shr.scl[qq][2] ;
            s = shr.sft[qq]    ;
            apply_yshear( a,b,s , nx,ny,nz , vol , (qq==0)? tm : NULL ) ;
         break ;

         case 2:
            a = shr.scl[qq][0] ;
            b = shr.scl[qq][1] ;
            s = shr.sft[qq]    ;
            apply_zshear( a,b,s , nx,ny,nz , vol , (qq==0)? tm : NULL ) ;
         break ;
      }
   }

   return ;
}

/*---------------------------------------------------------------------------
  Rotate and translate a 3D volume.
-----------------------------------------------------------------------------*/

void THD_rota_vol_byte( int   nx   , int   ny   , int   nz   ,
                        float xdel , float ydel , float zdel , DTYPE * vol ,
                        int ax1,float th1, int ax2,float th2, int ax3,float th3,
                        int dcode , float dx , float dy , float dz , Tmask * tm )
{
   MCW_3shear shr ;

   if( nx < 2 || ny < 2 || nz < 2 || vol == NULL ) return ;

   if( xdel == 0.0 ) xdel = 1.0 ;
   if( ydel == 0.0 ) ydel = 1.0 ;
   if( zdel == 0.0 ) zdel = 1.0 ;

   if( th1 == 0.0 && th2 == 0.0 && th3 == 0.0 ){  /* nudge rotation */
      th1 = 1.e-6 ; th2 = 1.1e-6 ; th3 = 0.9e-6 ;
   }

   shr = rot_to_shear( ax1,-th1 , ax2,-th2 , ax3,-th3 ,
                       dcode,dx,dy,dz , xdel,ydel,zdel ) ;

   if( ! ISVALID_3SHEAR(shr) ){
      fprintf(stderr,"*** THD_rota_vol_byte: can't compute shear transformation!\n") ;
      return ;
   }

   /*****************************************/

   apply_3shear( shr , nx,ny,nz , vol , tm ) ;

   /*****************************************/

   return ;
}

#if 0
/****************************************************************************
  Alternative entries, with rotation specified via a 3x3 matrix
  and shift as a 3-vector -- RWCox - 16 July 2000
*****************************************************************************/

/*---------------------------------------------------------------------------
  Rotate and translate a 3D volume
-----------------------------------------------------------------------------*/

#undef CLIPIT

void THD_rota_vol_matvec_byte( int   nx   , int   ny   , int   nz   ,
                               float xdel , float ydel , float zdel , DTYPE * vol ,
                               THD_mat33 rmat , THD_fvec3 tvec , Tmask * tm )
{
   MCW_3shear shr ;
   int dcode ;

   if( nx < 2 || ny < 2 || nz < 2 || vol == NULL ) return ;

   if( xdel == 0.0 ) xdel = 1.0 ;
   if( ydel == 0.0 ) ydel = 1.0 ;
   if( zdel == 0.0 ) zdel = 1.0 ;

   shr = rot_to_shear_matvec( rmat , tvec , xdel,ydel,zdel ) ;

   if( ! ISVALID_3SHEAR(shr) ){
      fprintf(stderr,"*** THD_rota_vol_byte: can't compute shear transformation!\n") ;
      return ;
   }

   /************************************/

   apply_3shear( shr , nx,ny,nz , vol , tm ) ;

   /************************************/

   return ;
}
#endif
