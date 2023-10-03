#
# SPDX-License-Identifier: BSD-3-Clause
# mrv2 
# Copyright Contributors to the mrv2 Project. All rights reserved.

#
# This demo loads a movie clip, plays it for a second and then stops it.
# 

from mrv2 import cmd, media, timeline

if len(media.list()) == 0:
    print("Please load a movie file or sequence with at least 100 frames.")
else:
    #
    # Start the playback
    #
    timeline.playForwards()

    #
    # Wait in a loop updating the UI for 5 seconds
    #
    time = 0.0
    while time < 5.0:
        time += cmd.update()
    
    #
    # Stop the playback
    #
    timeline.stop()
    
    #
    # Seek to start frame + 100
    #
    inOutRange = timeline.inOutRange()
    start = inOutRange.start_time.to_frames()
    
    timeline.seek( start + 100)
