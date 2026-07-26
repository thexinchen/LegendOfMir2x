#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "mainwindow.hpp"

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    try{
        MainWindow app;
        return app.run();
    }
    catch(const std::exception &e){
        std::cerr << "pkgviewer: " << e.what() << std::endl;
        return 1;
    }
}
