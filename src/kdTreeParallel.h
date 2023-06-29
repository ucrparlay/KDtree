#pragma once

#include "utility.h"

#define LOG std::cout
#define ENDL std::endl

constexpr int dims = 15; // works for any constant dimension
using idx = int;         // index of point (int can handle up to 2^31 points)
using coord = long long; // type of each coordinate
using coords = std::array<coord, dims>; // a coord array with length dims
struct point
{
   idx id;
   coords pnt;
};
struct pointComparator
{
   pointComparator( size_t index ) : index_( index ) {}
   bool
   operator()( const point& p1, const point& p2 ) const
   {
      return p1.pnt[index_] < p2.pnt[index_];
   }
   size_t index_;
};

using points = parlay::sequence<point>;

// max leaf size of tree
constexpr int node_size_cutoff = 16;

// **************************************************************
// bounding box (min value on each dimension, and max on each)
// **************************************************************
using box = std::pair<coords, coords>;

// TODO: handle double precision

// Given two points, return the min. value on each dimension
// minv[i] = smaller value of two points on i-th dimension
auto minv = []( coords a, coords b )
{
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = std::min( a[i], b[i] );
   return r;
};

auto maxv = []( coords a, coords b )
{
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = std::max( a[i], b[i] );
   return r;
};

coords
center( box& b )
{
   coords r;
   for( int i = 0; i < dims; i++ )
      r[i] = ( b.first[i] + b.second[i] ) / 2;
   return r;
}

box
bound_box( const parlay::sequence<point>& P )
{
   auto pts = parlay::map( P, []( point p ) { return p.pnt; } );
   auto x = box{ parlay::reduce( pts, parlay::binary_op( minv, coords() ) ),
                 parlay::reduce( pts, parlay::binary_op( maxv, coords() ) ) };
   return x;
}

box
bound_box( const box& b1, const box& b2 )
{
   return box{ minv( b1.first, b2.first ), maxv( b1.second, b2.second ) };
}

// **************************************************************
// Tree structure, leafs and interior extend the base node class
// **************************************************************
struct node
{
   bool is_leaf;
   idx size;
   box bounds;
   node* parent;
};

struct leaf : node
{
   points pts;
   leaf( points pts )
       : node{ true, static_cast<idx>( pts.size() ), bound_box( pts ),
               nullptr },
         pts( pts )
   {
   }
};

struct interior : node
{
   node* left;
   node* right;
   coord split;
   interior( node* _left, node* _right, coord _split )
       : node{ false, _left->size + _right->size,
               bound_box( _left->bounds, _right->bounds ), nullptr },
         left( _left ), right( _right ), split( _split )
   {
      left->parent = this;
      right->parent = this;
   }
};

parlay::type_allocator<leaf> leaf_allocator;
parlay::type_allocator<interior> interior_allocator;

template <typename slice>
node*
build( slice P, int dim, const int DIM )
{
   int n = P.size();
   if( n <= node_size_cutoff )
   {
      return leaf_allocator.allocate( parlay::to_sequence( P ) );
   }
   std::nth_element( P.begin(), P.begin() + n / 2, P.end(),
                     pointComparator( dim ) );
   dim = ( dim + 1 ) % DIM;

   node *L, *R;
   // parlay::par_do( [&]() { L = build( P.cut( 0, n / 2 ), dim, DIM ); },
   //                 [&]() { R = build( P.cut( n / 2, n ), dim, DIM ); } );
   L = build( P.cut( 0, n / 2 ), dim, DIM );
   R = build( P.cut( n / 2, n ), dim, DIM );
   return interior_allocator.allocate( L, R, P[n / 2].pnt[dim] );
}

//*-------------------for query-----------------------

coord
ppDistanceSquared( const point& p, const point& q, int DIM )
{
   coord r = 0, diff = 0;
   for( int i = 0; i < DIM; i++ )
   {
      diff = p.pnt[i] - q.pnt[i];
      r += diff * diff;
   }
   return r;
}

void
k_nearest( node* T, const point& q, int dim, const int DIM,
           kBoundedQueue<coord>& bq )
{
   coord d, dx, dx2;
   if( T->is_leaf )
   {
      leaf* TL = static_cast<leaf*>( T );
      for( int i = 0; i < TL->size; i++ )
      {
         d = ppDistanceSquared( q, TL->pts[i], DIM );
         bq.insert( d );
      }
      return;
   }

   interior* TI = static_cast<interior*>( T );
   dx = TI->split - q.pnt[dim];
   dx2 = dx * dx;

   dim = ( dim + 1 ) % DIM;

   k_nearest( dx > 0 ? TI->left : TI->right, q, dim, DIM, bq );
   if( dx2 > bq.top() && bq.full() )
   {
      return;
   }
   k_nearest( dx > 0 ? TI->right : TI->left, q, dim, DIM, bq );
}
