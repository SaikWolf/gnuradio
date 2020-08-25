/*
 * Copyright 2020 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually edited  */
/* The following lines can be configured to regenerate this file during cmake      */
/* If manual edits are made, the following tags should be modified accordingly.    */
/* BINDTOOL_GEN_AUTOMATIC(0)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(mmse_interp_differentiator_cc.h) */
/* BINDTOOL_HEADER_FILE_HASH(7aceb77ae6aefbaff71960b27310ffe1)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/filter/mmse_interp_differentiator_cc.h>
// pydoc.h is automatically generated in the build directory
#include <mmse_interp_differentiator_cc_pydoc.h>

void bind_mmse_interp_differentiator_cc(py::module& m)
{

    using mmse_interp_differentiator_cc = ::gr::filter::mmse_interp_differentiator_cc;


    py::class_<mmse_interp_differentiator_cc,
               std::shared_ptr<mmse_interp_differentiator_cc>>(
        m, "mmse_interp_differentiator_cc", D(mmse_interp_differentiator_cc))

        .def(py::init<>(),
             D(mmse_interp_differentiator_cc, mmse_interp_differentiator_cc, 0))


        .def("ntaps",
             &mmse_interp_differentiator_cc::ntaps,
             D(mmse_interp_differentiator_cc, ntaps))


        .def("nsteps",
             &mmse_interp_differentiator_cc::nsteps,
             D(mmse_interp_differentiator_cc, nsteps))


        .def("differentiate",
             &mmse_interp_differentiator_cc::differentiate,
             py::arg("input"),
             py::arg("mu"),
             D(mmse_interp_differentiator_cc, differentiate))

        ;


    py::module m_kernel = m.def_submodule("kernel");
}