#include "utility.h"

const int MAX_DIM = 15;

template <typename T>
class Point
{
 public:
   void
   print()
   {
      for( int i = 0; i < 2; i++ )
      {
         std::cout << x[i] << " ";
      }
   };
   void
   print( int d )
   {
      for( int i = 0; i < d; i++ )
      {
         std::cout << x[i] << " ";
      }
   }

 public:
   T x[MAX_DIM];
};

template <typename T>
struct PointCompare
{
 public:
   PointCompare( size_t index ) : index_( index ) {}
   bool
   operator()( const Point<T>& n1, const Point<T>& n2 ) const
   {
      return n1.x[index_] < n2.x[index_];
   }
   size_t index_;
};

template <typename T>
inline T
dist( Point<T>* a, Point<T>* b, int dim )
{
   double t, d = 0.0;
   while( dim-- )
   {
      t = a->x[dim] - b->x[dim];
      d += t * t;
   }
   return d;
}

template <typename T>
class KDnode
{
 public:
   void
   print()
   {
      std::cout << isLeaf << " " << num << std::endl;
      for( int i = 0; i < num; i++ )
      {
         p[i].print();
         puts( "" );
      }
   }

 public:
   KDnode(){};
   Point<T>* p;
   KDnode<T>*left = nullptr, *right = nullptr;
   bool isLeaf = false;
   int num = 1;
};

template <typename T>
class KDtree
{
 public:
   KDtree() {}

   void
   init( const int& _DIM, const int& _LEAVE_WRAP, Point<T>* a, int len );

   KDnode<T>*
   make_tree( Point<T>* a, int len, int i );

   void
   k_nearest( KDnode<T>* root, Point<T>* nd, int i );

   double
   query_k_nearest( Point<T>* nd, int _K )
   {
      this->K = _K;
      this->k_nearest( this->KDroot, nd, 0 );
      double ans = q.top();
      this->reset();
      return ans;
   };

   void
   reset()
   {
      while( !q.empty() )
         q.pop();
   }

 private:
   int DIM;
   int K;
   int LEAVE_WRAP = 16;
   KDnode<T>* KDroot;

   kArrayQueue<T> kq;
   std::priority_queue<T> q;
};