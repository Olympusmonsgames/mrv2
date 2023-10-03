// SPDX-License-Identifier: BSD-3-Clause
// mrv2
// Copyright Contributors to the mrv2 Project. All rights reserved.

#pragma once

#include <memory>
#include <vector>

#include <tlCore/Time.h>

#include "mrvDraw/Shape.h"
#include "mrvNetwork/mrvMessage.h"

namespace mrv
{
    namespace draw
    {

        class Annotation
        {
        public:
            Annotation(){};
            Annotation(const otime::RationalTime& frame, const bool allFrames);
            ~Annotation();

            bool empty() const;

            void push_back(const std::shared_ptr< Shape >&);
            std::shared_ptr< Shape > lastShape() const;

            //! Remove shape without keeping it in the undo buffer.
            //! Used for laser shapes.
            void remove(const std::shared_ptr< Shape >&);

            void undo();
            void redo();

        public:
            otime::RationalTime time = time::invalidTime;
            std::vector< std::shared_ptr< Shape > > shapes;
            std::vector< std::shared_ptr< Shape > > undo_shapes;
            bool allFrames = false;
        };

        void to_json(nlohmann::json& json, const Annotation& value);
        void from_json(const nlohmann::json& json, Annotation& value);

        std::shared_ptr< Annotation >
        messageToAnnotation(const mrv::Message& m);

    } // namespace draw

} // namespace mrv
