/*
 * Same as summa_abt, but local block of A is assumed to be stored
 * in transposed form.
 */
#include <stdio.h>
#include "global.h"
#include "sndrcv.h"
#include "f2cblas.h"

                /* macro for column major indexing                 */
#define A( i,j ) (a[ j*lda + i ])
#define B( i,j ) (b[ j*ldb + i ])
#define C( i,j ) (c[ j*ldc + i ])

#define min( x, y ) ( (x) < (y) ? (x) : (y) )

void summa_abt2( m, n, k, nb, alpha, a, lda, b, ldb, 
             beta, c, ldc, m_a, n_a, m_b, n_b, m_c, n_c, 
             myrow, mycol, nprow, npcol, proclist, work1, work2 )

Integer    m, n, k,         /* global matrix dimensions                */
       nb,              /* panel width                             */
       m_a[], n_a[],    /* dimensions of blocks of A               */
       m_b[], n_b[],    /* dimensions of blocks of B               */
       m_c[], n_c[],    /* dimensions of blocks of C               */
       myrow, mycol,     /* my  row and column index                */
       nprow, npcol,     /* number of node rows and columns         */
       *proclist,        /* list of nprow * npcol processor ids to use */
       lda, ldb, ldc;   /* leading dimension of local arrays that 
                           hold local portions of matrices A, B, C */
DoublePrecision *a, *b, *c,      /* arrays that hold local parts of A, B, C */
       alpha, beta,     /* multiplication constants                */
       *work1, *work2;  /* work arrays                             */
{

  static Integer    i_one=1;         /* used for constant passed to blas call   */
  static DoublePrecision d_one=1.0, d_zero=0.0;

  Integer i, j, kk, iwrk,
      icurrow, icurcol, /* index of row and column that hold current 
                           row and column, resp., for rank-1 update*/
      ii, jj;           /* local index (on icurrow and icurcol, resp.)
                           of row and column for rank-1 update     */

  Integer  me, row_to, row_from, col_to, col_from, mes_type,
           first_in_row, last_in_row, first_in_col, last_in_col,
           msg_id_row, msg_id_col;

  DoublePrecision   *p;

  extern void RING_Bcast(), RING_SUM();

  /* get ids for processors to send to/receive from in row/column */

  me = mycol * nprow + myrow;

  first_in_row = myrow;
  last_in_row  = nprow * ( npcol - 1 ) + myrow;

  row_to   = me+nprow;
  row_from = me-nprow;

  if( row_to   > last_in_row )  row_to   = first_in_row;
  if( row_from < first_in_row ) row_from = last_in_row;

  first_in_col = mycol * nprow;
  last_in_col  = first_in_col + nprow - 1;

  col_to   = me+1;
  col_from = me-1;

  if( col_to   > last_in_col )  col_to   = first_in_col;
  if( col_from < first_in_col ) col_from = last_in_col;

  row_to   = *(proclist+row_to);
  row_from = *(proclist+row_from);
  col_to   = *(proclist+col_to);
  col_from = *(proclist+col_from);

  /* scale local block of C   */

  for ( j=0; j<n_c[ mycol ]; j++ )
    for ( i=0; i<m_c[ myrow ]; i++ )
      C( i,j ) = beta * C( i,j );

  icurrow = 0;
  icurcol = 0;
  ii = jj = 0;

  msg_id_row = 1;
  msg_id_col = 2;

  me = proclist[ me ];

  for ( kk=0; kk<n; kk+=iwrk) {

    iwrk = min( nb, m_b[ icurrow ]-ii );
    iwrk = min( iwrk, n_c[ icurcol ]-jj );

    /* pack current iwrk rows of B into work2          */
    if ( myrow == icurrow ) 
       dlacpy_( "General", &iwrk, &n_b[ mycol ], &B( ii, 0 ), &ldb, work2, 
                &iwrk );

    RING_Bcast( work2, n_b[ mycol ]*iwrk, proclist[mycol*nprow+icurrow], me,
                col_to, col_from, msg_id_col ); 

    dgemm_( "Transpose", "Transpose", &m_c[ myrow ], &iwrk, &n_a[ mycol ],
            &alpha, a, &lda, work2, &iwrk, &d_zero, work1, &m_c[myrow], 1, 1 );

    RING_SUM( work1, m_c[ myrow ]*iwrk, proclist[icurcol*nprow+myrow], me,
              row_to, row_from, msg_id_row, work2 ); 

    if( mycol == icurcol ) {
      p = work1;
      for( j=jj; j<jj+iwrk; j++ ) {
         daxpy_( &m_c[ myrow ], &d_one, p, &i_one, &C( 0,j ), &i_one );
         p += m_c[ myrow ];
      }
    }

                 /* update icurcol, icurrow, ii, jj                 */
    msg_id_row += 4;
    msg_id_col += 4;

    if( msg_id_row > 32765 ) msg_id_row = 1;
    if( msg_id_col > 32765 ) msg_id_col = 2;

    /* update icurcol, icurrow, ii, jj                 */
    ii += iwrk;
    jj += iwrk;
    if ( jj>=n_c[ icurcol ] ) { icurcol++; jj = 0; };
    if ( ii>=m_b[ icurrow ] ) { icurrow++; ii = 0; };
  }
}
