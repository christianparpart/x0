/* <plugins/director/HaproxyApi.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include "Backend.h"
#include "Director.h"
#include "HealthMonitor.h"

#include <x0/Buffer.h>
#include <x0/TimeSpan.h>
#include <x0/CustomDataMgr.h>
#include <x0/http/HttpRequest.h>


class HaproxyApi :
    public x0::CustomData
{
private:
    typedef std::unordered_map<std::string, Director*> DirectorMap;

    DirectorMap* directors_;

public:
    explicit HaproxyApi(DirectorMap* directors);
    ~HaproxyApi();

    void monitor(x0::HttpRequest* r);
    void stats(x0::HttpRequest* r, const std::string& prefix);

private:
    void csv(x0::HttpRequest* r);
    void buildFrontendCSV(Buffer& buf, Director* director);
};

