// CMakeProject1.cpp: definiuje punkt wejścia dla aplikacji.
//

#include "Server.h"
#include <ServerManager/ServerManager.h>

using namespace std;

int main()
{
	ServerManager srvman(5555,"127.0.0.1");
	return 0;
}
