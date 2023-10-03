// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#include <iostream>

#ifdef MRV2_PYBIND11
#    include <pybind11/embed.h>
namespace py = pybind11;

#    include "mrvPy/Cmds.h"
#endif

#include "mrvFl/mrvInit.h"

#include "tlGL/Init.h"

#include "mrvApp/App.h"

#ifdef MRV2_PYBIND11
PYBIND11_EMBEDDED_MODULE(mrv2, m)
{
    mrv2_enums(m);
    mrv2_vectors(m);
    mrv2_otio(m);
    mrv2_filepath(m);
    mrv2_fileitem(m);
    mrv2_filesmodel(m);
    mrv2_image(m);
    mrv2_media(m);
    mrv2_settings(m);
    mrv2_timeline(m);
    mrv2_playlist(m);
    mrv2_io(m);
    mrv2_annotations(m);
#    ifdef TLRENDER_USD
    mrv2_usd(m);
#    endif
    mrv2_commands(m);
    mrv2_python_plugins(m);
}
#endif

int main(int argc, char* argv[])
{
    int r = 1;
    try
    {
#ifdef MRV2_PYBIND11
        // start the interpreter and keep it alive
        py::scoped_interpreter guard{};
#endif
        auto context = tl::system::Context::create();
        mrv::init(context);
        mrv::App app(argc, argv, context);
        r = app.getExit();
        if (0 == r)
        {
            r = app.run();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
    }
    return r;
}
