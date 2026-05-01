#pragma once

#include "logging/logging.hpp"


#include <crow.h>
#include <crow/middlewares/cors.h>

using App = crow::App<
    OptionsLogger, 
    AccessLogger, 
    crow::CORSHandler
>;