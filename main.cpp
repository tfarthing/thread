import cpp.thread;

#include <chrono>
#include <cstdio>

int main(int argc, char** argv)
{
	try
	{
		cpp::Thread t = []( )
		{
			cpp::Thread::sleep( std::chrono::seconds{ 15 } );
		};
		t.interrupt( );
		t.join( );
	}
	catch ( std::exception & e )
	{
		printf( e.what( ) );
	}

	return 0;
}