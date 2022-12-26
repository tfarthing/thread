import cpp.thread;

#include <chrono>
#include <cstdio>
#include <string>
#include <map>



class ThreadObjectBase
{
public:
	virtual ~ThreadObjectBase( ) { }
	virtual void * ptr( ) = 0;
};


template<class T>
class ThreadObject
	: public ThreadObjectBase
{
public:
	ThreadObject( )
		{ }
	ThreadObject( const T & copy )
		: data( copy ) { }
	~ThreadObject() override
		{ }
	void * ptr( ) override
		{ return &data; }
private:
	T data;
};


template<class T>
class ThreadLocal
{
public:
	ThreadLocal( bool inherit = false ) { }

	const T & get( );
	void set( const T & value );
	void remove( );
};


class ThreadData
{
public:
	template<class T>
	static const T & get( ThreadLocal<T> * threadLocal )
	{
		void * p = (void *)threadLocal;
		auto itr = map.find( p );
		if ( itr == map.end( ) )
		{
			map[p] = std::make_unique<ThreadObject<T>>( );
			itr = map.find( p );
		}
		T * value = (T *)itr->second->ptr( );
		return *value;
	}

	template<class T>
	static void set( ThreadLocal<T> * threadLocal, const T & value )
	{
		void * p = (void *)threadLocal;
		std::unique_ptr<ThreadObjectBase> d = std::make_unique<ThreadObject<T>>( value );
		map[p] = std::move(d);
	}

	template<class T>
	static void remove( ThreadLocal<T> * threadLocal )
	{
		void * p = (void *)threadLocal;
		map.erase( p );
	}

	static thread_local std::map<void *, std::unique_ptr<ThreadObjectBase>> map;
};

thread_local std::map<void *, std::unique_ptr<ThreadObjectBase>> ThreadData::map;

template<class T>
const T & ThreadLocal<T>::get( )
	{ return ThreadData::get( this ); }

template<class T>
void ThreadLocal<T>::set( const T & value )
	{ ThreadData::set( this, value ); }

template<class T>
void ThreadLocal<T>::remove( )
	{ ThreadData::remove( this ); }


int main(int argc, char** argv)
{
	ThreadLocal<int> var1;
	ThreadLocal<std::string> var2;
	int i = var1.get( );
	auto s = var2.get( );
	var1.set( 10 );
	var2.set( "hello" );

	try
	{
		cpp::Thread t = [&]( )
		{
			int j = var1.get( );
			var1.set( 11 );
			j = var1.get( );
			cpp::Thread::sleep( std::chrono::seconds{ 15 } );
		};
		t.interrupt( );
		t.join( );

		i = var1.get( );
	}
	catch ( std::exception & e )
	{
		printf( e.what( ) );
	}

	return 0;
}