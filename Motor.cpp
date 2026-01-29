#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>

#include "Motor.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "WorkerUtils.hpp"
#include "globals.hpp"
#include "imp_hal.hpp"
#include "Config.hpp"
#include <unistd.h>
#include <fcntl.h>

#define MOTOR_DEBUG

#define SV_SOCK_PATH "/dev/md"
#define BUF_SIZE 15

#define PID_SIZE 32

#define MOTOR_INVERT_X 0x1
#define MOTOR_INVERT_Y 0x2
#define MOTOR_INVERT_BOTH 0x3

namespace Motor {

int serverfd = 0;

void JSON_initial(motor_message message)
{
  // return all known parameters in JSON string
  // idea is when client page loads in browser we
  // get current details from camera
    LOG_DEBUG("JASON status: " << (message).status << ", pos X,Y: (" << (message).x << "," << (message).y 
        << ")- Max X, Y: {" << (message).x_max_steps << "," << (message).y_max_steps << "}-> Speed: " 
        << (message).speed << ", inversion: " << (message).inversion_state);
}

void JSON_status(motor_message message)
{
  // return xpos,ypos and status in JSON string
  // allows passing straight back to async call from ptzclient.cgi
  // with little effort and ability to track x,y position
  LOG_DEBUG("JASON status: " << (message).status << ", X,Y: (" << (message).x << "," << (message).y << ")-> speed: " 
    << (message).speed << ", inversion: " << (message).inversion_state);
}

void xy_pos(motor_message message)
{
    LOG_DEBUG("X,Y: " << (message).x << "," << (message).y);
}

void show_status(motor_message message)
{
#ifdef MOTOR_DEBUG
    LOG_DEBUG("Max X,Y Steps: (" << (message).x_max_steps << "," << (message).y_max_steps << "), Status Move:" 
        << (message).status << ", X, Y Steps: (" 
        << (message).x << "," << (message).y << ")");

  // Report motor inversion status
  if (message.inversion_state == MOTOR_INVERT_BOTH) {
      LOG_DEBUG("Motor Inversion: BOTH X and Y are inverted");
  } else if (message.inversion_state == MOTOR_INVERT_X) {
      LOG_DEBUG("Motor Inversion: X axis is inverted");
  } else if (message.inversion_state == MOTOR_INVERT_Y) {
      LOG_DEBUG("Motor Inversion: Y axis is inverted");
  } else {
      LOG_DEBUG("Motor Inversion: OFF");
  }
#endif 
}

void print_request_message(request *req)
{
#ifdef MOTOR_DEBUG
    LOG_DEBUG("Sent message - command:" << std::string (1,req->command) << "," << std::string (1,req->type) 
        << ",(" <<  req->x << "," <<  req->y << "), ->" <<  req->speed << "," <<  req->speed_supplied);
#endif     
}

int check_daemon(char *file_name)
{
    FILE *f;
    long pid;
    char pid_buffer[PID_SIZE];

    f = fopen(file_name, "r");
    if(f == NULL)
        return 0;

    if (fgets(pid_buffer, PID_SIZE, f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);

    if (sscanf(pid_buffer, "%ld", &pid) != 1) {
        return 0;
    }

    if (kill(pid, 0) == 0) {
        return 1;
    }

    return 0;
}

void initialize_request_message(request *req) {
    memset(req, 0, sizeof(request));
    req->command = 'd'; // Default command
    req->type = 's'; // Default type
    req->x = 30;
    req->got_x = 0;
    req->y = 0;
    req->got_y = 0;
    req->speed = 0;
    req->speed_supplied = false;
}

int motor_init()
{
  char *daemon_pid_file;

    //openlog ("motors app", LOG_PID, LOG_USER);
    daemon_pid_file = "/var/run/motors-daemon";
    if (check_daemon(daemon_pid_file) == 0) {
        LOG_DEBUG("Motors daemon is NOT running, please start the daemon\n");
        return -1;
    }
  //should open socket here
  sockaddr_un addr;

  int serverfd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (serverfd == -1) {
      return -1;
  }
  memset(&addr, 0, sizeof(sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SV_SOCK_PATH, sizeof(addr.sun_path) - 1);

  //connect to the socket
  if (connect(serverfd, (sockaddr *) &addr,sizeof(sockaddr_un)) == -1)
      return -1;

  return serverfd;
}

int send_request(request request_message) {
    serverfd = motor_init();

    if (serverfd == -1) {
        LOG_DEBUG("Motor socket access failed!");
        return -1;
    }
    LOG_DEBUG("EXCUTE MOTOR ACTION COMMAND! ");
    print_request_message(&request_message);
    write(serverfd,&request_message,sizeof(request));
    return 0;
}
  
int motor_action(char cmd, int value1) {
    char direction = '\0';
    int stepspeed = 900;
    request request_message;

    initialize_request_message(&request_message);
    request_message.command = cmd;
#ifdef MOTOR_DEBUG
    LOG_DEBUG("motor command: <" << std::string (1, cmd) << "> value: " << value1);
#endif
    switch (cmd)
    {
        case MOTOR_DIR: //'d'
            direction = static_cast<char>(value1);
            request_message.type = direction;
#ifdef MOTOR_DEBUG
            LOG_DEBUG("motor command: MOTOR_DIR");
#endif
                    switch (direction) 
            {
                case MOTOR_STOP: // stop
                    request_message.type = 's';
                    break;
                case MOTOR_CRUISE: // cruise
                    request_message.type = 'c';
                    break;
                case MOTOR_HOME: // go back 'h'
                    request_message.type = 'b';
                    break;
                case MOTOR_ABS_POS: // set position (absolute movement)
                    request_message.type = 'h';
                    break;
                case MOTOR_REL_POS: // move x y (relative movement)
                    request_message.type = 'g';
                    break;
                default:
                LOG_DEBUG("Invalid Direction Argument: " <<  direction);
            }
            request_message.x = cfg->motor.x;
            request_message.y = cfg->motor.y;
            request_message.speed = cfg->motor.speed;
            request_message.speed_supplied = cfg->motor.speed_supplied;
            
            send_request(request_message);
            break;
        case MOTOR_SET_SPEED_STEP: //'s';
            stepspeed = value1;
            request_message.speed = stepspeed;
            request_message.speed_supplied = true; // Set speed_supplied to true when speed is provided
            cfg->motor.speed_supplied = true;
            send_request(request_message);
            if (request_message.speed_supplied) {
                request_message.speed = stepspeed;
                cfg->motor.speed = stepspeed;
            } else {
                request_message.speed = 0;  // Indicate that speed is not set
            }
            break;
        case MOTOR_X_POS: // 'X', set only
            request_message.x = value1;
            request_message.got_x = 1;
            request_message.command = '\0';
            cfg->motor.x = value1;
            break;
        case MOTOR_Y_POS:  // 'Y', set only
            request_message.y = value1;
            request_message.got_y = 1;
            request_message.command = '\0';
            cfg->motor.y = value1;
            break;
        case MOTOR_JSON_STATUS: // 'j'
            send_request(request_message);
            motor_message status;
            read(serverfd,&status,sizeof(motor_message));

            JSON_status(status);
            break;
        case MOTOR_JSON_ALL:  // 'i'
            // get all initial values
            send_request(request_message);
            motor_message initial;
            read(serverfd,&initial,sizeof(motor_message));

            JSON_initial(initial);
            break;
        case MOTOR_GET_POSITION:  // 'p'
            send_request(request_message);
            motor_message pos;
            read(serverfd,&pos,sizeof(motor_message));

            xy_pos(pos);
            break;
        case MOTOR_INIT: // reset 'r'
            send_request(request_message);
            break;
        case MOTOR_STATUS: // status'S';
            send_request(request_message);
            motor_message stat;
            read(serverfd,&stat,sizeof(motor_message));

            show_status(stat);
            break;
        case MOTOR_INV_X: // Invert motor "m"
            request_message.command = 'I';
            request_message.type = 'x'; // Invert X
            send_request(request_message);
            break;
        case MOTOR_INV_Y: // Invert motor "n"
            request_message.command = 'I';
            request_message.type = 'y'; // Invert Y
            send_request(request_message);
            break;
        case MOTOR_INV_XY:  // "M" both XY
            request_message.command = 'I';
            request_message.type = 'b'; // Invert Y
            send_request(request_message);
            break;
        case MOTOR_IS_BUSY: // is moving?  'b';
            send_request(request_message);
            motor_message busy;
            read(serverfd,&busy,sizeof(motor_message));
            if(busy.status == MOTOR_IS_RUNNING){
                //LOG_DEBUG("1");
                break;
            } else{
                //LOG_DEBUG("0");
                break;
            }
        default:
            LOG_DEBUG("Invalid MOTOR COMMAND:  " << cmd);
            break;
    }

  return 0;
}

} // namespace Motor 
