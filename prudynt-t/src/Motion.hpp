#include "imp_hal.hpp"
#ifndef Motion_hpp
#define Motion_hpp

#include <memory>
#include <thread>
#include <atomic>
#include "Config.hpp"
#include "Logger.hpp"
#include "globals.hpp"
#include "imp/imp_system.h"
#include "imp/imp_ivs.h"
#include "imp/imp_ivs_move.h"

#include <vector>
#include <string>

class Motion {
    public:

        struct box_region {
            int hit = 0;
            int h = 0;
            int v = 0;
        };
        
        box_region region[52];

        void detect();
        static void *run(void* arg);
        int init();
        int exit();

    private:
        int ivsChn = 0;
        int ivsGrp = 0;


         enum weight_index {
            CENTER,
            LEFT,
            RIGHT,
            UP,
            DOWN,
            UP_LEFT,
            UP_RIGHT,
            DOWN_LEFT,
            DOWN_RIGHT
        };
        
        /*   --  Motion Heat Map -- 8x6
        . _0__1__2__3__4__5__6__7_ .  
        --|  0  1  2  3  4  5  6  7  |-0-
        --|  8  9 10 11 12 13 14 15  |-1-
        --| 16 17 18 19 20 21 22 23  |-2-
        --| 24 25 26 27 28 29.30.31  |-3-
        --| 32 33 34 35 36 37 38 39  |-4-
        --| 40 41 42 43 44 45 46 47  |-5-
        . ------------------------ .  
        */
        std::vector<std::vector<int>> box_Matrix = {
            {19, 20, 27, 28, 12, 13, 35, 36, 18, 26, 21, 29},  // center, stable area
            {16,17,18,24,25,26},            // left
            {21,22,23,29,30,31},            // right
            {2,3,4,5,10,11,12,13},          // up
            {34,35,36,37,42,43,44,45},      // down
            {0,1,2,8,9,10},                 // up_left
            {5,6,7,13,14,15},               // up_right
            {32,33,34,40,41,42},            // down left
            {37,38,39,45,46,47}             // down right
        };

        int box_weight[9];

        std::string getConfigPath(const char *itemName);

        std::atomic<bool> moving;
        std::atomic<bool> indicator;
        IMP_IVS_MoveParam move_param;
        IMPIVSInterface *move_intf;
        std::thread detect_thread;

        IMPCell fs = {};
        IMPCell ivs_cell = {};

        IMPEncoderCHNAttr channelAttributes;

        int box2region(int x0, int y0, int x1, int y1);
        int trackTo(int x, int y);
        int resetToHome();
        int motorMoving();

        int trackCalculate(int x, int y);
        int hit2hv(box_region *region, int hit, int nh);

};

#endif /* Motion_hpp */
