#ifndef MOTOR_HPP
#define MOTOR_HPP

namespace Motor {

    enum motor_status
    {
        MOTOR_IS_STOP,
        MOTOR_IS_RUNNING,
    };

    // command:  d-move,r-reset,s-set speed,p-get position,b-is busy,S-Status,i-initial,j-JSON 
    #define MOTOR_RESET 'r'    // r
    #define MOTOR_DIR 'd'  // move
    #define MOTOR_SET_SPEED_STEP 's'  // s
    #define MOTOR_GET_POSITION 'p'
    #define MOTOR_X_POS 'X'
    #define MOTOR_Y_POS 'Y'
    #define MOTOR_X_STEP 'x'
    #define MOTOR_Y_STEP 'y'
    #define MOTOR_IS_BUSY 'b'  // b
    #define MOTOR_STATUS 'S'   // S
    #define MOTOR_INIT 'i'     // i
    #define MOTOR_JSON_STATUS 'j'  // j
    #define MOTOR_JSON_ALL 'J'
    #define MOTOR_INV_X 'm'
    #define MOTOR_INV_Y 'n'
    #define MOTOR_INV_XY 'M'
    #define MOTOR_SWAP 'k'

    // type(direction):   h-absolute,g-relative,c-cruise,s-stop,b-home
    #define MOTOR_STOP 's'    // s
    #define MOTOR_CRUISE 'c'   // c
    #define MOTOR_HOME 'b'
    #define MOTOR_ABS_POS 'h'  // g
    #define MOTOR_REL_POS 'g'

    struct request{
        char command; // d,r,s,p,b,S,i,j (move, reset,set speed,get position, is busy,Status,initial,JSON)
        char type;   // g,h,c,s (absolute,relative,cruise,stop)
        int x;
        int got_x;
        int y;
        int got_y;
        int speed;  // Add speed to the request structure
        int speed_supplied;
    };

    struct motor_status_st
    {
        int directional_attr;
        int total_steps;
        int current_steps;
        int min_speed;
        int cur_speed;
        int max_speed;
        int move_is_min;
        int move_is_max;
    };

    struct motor_message
    {
        int x;
        int y;
        motor_status status;   // stop, running
        int speed;
        /* these two members are not standard from the original kernel module */
        unsigned int x_max_steps;
        unsigned int y_max_steps;
        unsigned int inversion_state; // Report the inversion state
    };

    struct motors_steps
    {
        int x;
        int y;
    };

    struct motor_reset_data
    {
        unsigned int x_max_steps;
        unsigned int y_max_steps;
        unsigned int x_cur_step;
        unsigned int y_cur_step;
    };

    //static void *run(void* arg);
    int motor_init();
    int motor_action(char cmd, int value1);
    // int exit();

    void JSON_initial(struct motor_message *message);
    void JSON_status(struct motor_message *message);
    void xy_pos(struct motor_message *message);
    void show_status(struct motor_message *message);
    int check_daemon(char *file_name);
    void print_request_message(struct request *req);
    void initialize_request_message(struct request *req);
    int send_request(request request_message);
};

#endif // MOTOR_HPP