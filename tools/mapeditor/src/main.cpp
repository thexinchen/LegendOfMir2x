#include <exception>
#include <iostream>

#include "mainwindow.hpp"

int main()
{
    try{
        MainWindow app;
        return app.run();
    }
    catch(const std::exception &e){
        std::cerr << "mapeditor: " << e.what() << std::endl;
        return 1;
    }
}
