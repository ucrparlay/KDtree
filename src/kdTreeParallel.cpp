#include "kdTreeParallel.h"

//@ Find Bounding Box
coords
center( box& b ) {
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = ( b.first[i] + b.second[i] ) / 2;
   return r;
}

box
bound_box( const parlay::sequence<point>& P ) {
   auto pts = parlay::map( P, []( point p ) { return p.pnt; } );
   auto x = box{ parlay::reduce( pts, parlay::binary_op( minv, coords() ) ),
                 parlay::reduce( pts, parlay::binary_op( maxv, coords() ) ) };
   return x;
}

box
bound_box( const box& b1, const box& b2 ) {
   return box{ minv( b1.first, b2.first ), maxv( b1.second, b2.second ) };
}

parlay::type_allocator<leaf> leaf_allocator;
parlay::type_allocator<interior> interior_allocator;

//@ Support Functions
inline coord
ppDistanceSquared( const point& p, const point& q, int DIM ) {
   coord r = 0, diff = 0;
   for( int i = 0; i < DIM; i++ ) {
      diff = p.pnt[i] - q.pnt[i];
      r += diff * diff;
   }
   return r;
}

template <typename T, typename slice>
std::array<T, PIVOT_NUM>
pick_pivot( slice A, size_t n, int dim ) {
   int size = PIVOT_NUM << 1 | 1; //* 2*PIVOT_NUM+1 size
   int arr[size];
   for( int i = 0; i < size; i++ ) {
      arr[i] = A[i * ( n / size )].pnt[dim]; //* sample in A
   }
   std::sort( arr, arr + size );
   std::array<T, PIVOT_NUM> pivots;
   for( int i = 0; i < PIVOT_NUM; i++ ) {
      pivots[i] = arr[i << 1 | 1]; //* pick PIVOT_NUM within cadidates
   }
   if( arr[0] == arr[1] ) { //% add disturb
      pivots[0] = pivots[1];
   } else if( arr[3] == arr[4] ) {
      pivots[1] = pivots[0];
   }
   return pivots;
}

//@ Parallel KD tree cores
template <typename slice>
node*
build( slice In, slice Out, int dim, const int DIM ) {
   int n = In.size(), ln, rn;
   coord split;

   if( n <= LEAVEWRAP ) {
      return leaf_allocator.allocate( parlay::to_sequence( In ) );
   }
   if( n <= SERIAL_BUILD_CUTOFF ) { //* serial run nth element
      std::nth_element( In.begin(), In.begin() + n / 2, In.end(),
                        pointLess( dim ) );
      ln = n / 2, rn = n - n / 2;
      split = In[n / 2].pnt[dim];
      std::swap( In, Out );
   } else { //* parallel partiton
      auto mid = parlay::kth_smallest( In, n / 2, pointLess( dim ) );
      split = mid->pnt[dim];
      // auto flag =
      //     parlay::map( In, [&]( point i ) { return i.pnt[dim] < split; } );
      ln = parlay::filter_into( In, Out,
                                [&]( point i ) { return i.pnt[dim] < split; } );
      rn = parlay::filter_into( In, Out.cut( ln, n ), [&]( point i ) {
         return i.pnt[dim] >= split;
      } );
      assert( ln + rn == n );
   }

   node *L, *R;
   dim = ( dim + 1 ) % DIM;
   parlay::par_do_if(
       n > SERIAL_BUILD_CUTOFF,
       [&]() { L = build( Out.cut( 0, ln ), In.cut( 0, ln ), dim, DIM ); },
       [&]() { R = build( Out.cut( ln, n ), In.cut( ln, n ), dim, DIM ); } );

   return interior_allocator.allocate( L, R, split );
}

//? parallel query
void
k_nearest( node* T, const point& q, int dim, const int DIM,
           kBoundedQueue<coord>& bq ) {
   coord d, dx, dx2;
   if( T->is_leaf ) {
      leaf* TL = static_cast<leaf*>( T );
      for( int i = 0; i < TL->size; i++ ) {
         d = ppDistanceSquared( q, TL->pts[i], DIM );
         bq.insert( d );
      }
      return;
   }

   interior* TI = static_cast<interior*>( T );
   dx = TI->split - q.pnt[dim];
   dx2 = dx * dx;

   if( ++dim >= DIM )
      dim = 0;

   k_nearest( dx > 0 ? TI->left : TI->right, q, dim, DIM, bq );
   if( dx2 > bq.top() && bq.full() ) {
      return;
   }
   k_nearest( dx > 0 ? TI->right : TI->left, q, dim, DIM, bq );
}

void
delete_tree( node* T ) { // delete tree in parallel
   if( T->is_leaf )
      leaf_allocator.retire( static_cast<leaf*>( T ) );
   else {
      interior* TI = static_cast<interior*>( T );
      parlay::par_do_if(
          T->size > 1000, [&] { delete_tree( TI->left ); },
          [&] { delete_tree( TI->right ); } );
      interior_allocator.retire( TI );
   }
}

//@ Template declation
template node*
build<parlay::slice<point*, point*>>( parlay::slice<point*, point*> In,
                                      parlay::slice<point*, point*> Out,
                                      int dim, const int DIM );
template std::array<coord, PIVOT_NUM>
pick_pivot( parlay::slice<point*, point*> A, size_t n, int dim );
