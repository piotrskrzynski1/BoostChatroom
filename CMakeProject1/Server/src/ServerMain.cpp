#include <Server/ServerManager.h>

using namespace std;


int main()
{
	ServerManager srvman(5555,5556,"127.0.0.1");
	srvman.StartServer();
	return 0;
}
