// C library headers
#include <stdio.h>
#include <string.h>
#include <iostream>

#include "parsers_manager.h"
  
// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()
#include <poll.h>

parsers_manager Manager;

using namespace std;

void uart_setup(int fd){
    struct termios termios_uart;
    
    if(tcgetattr(fd, &termios_uart) != 0){
        printf("error %i from tcgetattr: %s\n", errno, strerror(errno));
    }

    termios_uart.c_cflag &= ~ (PARENB | CSTOPB | CRTSCTS);
    termios_uart.c_cflag |= CS8 | CREAD | CLOCAL; // 8 bits per byte
    termios_uart.c_lflag &= ~ (ICANON | ECHO | ECHOE | ECHONL | ISIG);
    termios_uart.c_iflag &= ~(IXON | IXOFF | IXANY);
    termios_uart.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    termios_uart.c_oflag &= ~(OPOST | ONLCR);
    termios_uart.c_cc[VTIME] = 0;
    termios_uart.c_cc[VMIN] = 0;

    cfsetispeed(&termios_uart, B9600);
    cfsetospeed(&termios_uart, B9600);
    

    if (tcsetattr(fd, TCSANOW, &termios_uart) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    }
}

void fd_setup(pollfd * fds, int fd){
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
}


//int main(int argc, char* argv[]){       // argc - кількість переданих елементів (перший - завжди назва програми), argv - масив елементів
int main(){
//    if(argc < 2){ return 1; } 
    int fd = open(/*argv[1]*/"/dev/ttyS0", O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
    }
    uart_setup(fd);

    struct pollfd fds[1];
    fd_setup(fds, fd);
    
    char buff[1024];
    int head = 0;
    int tail = 0;
    int bytes = 0;
    
    while(1){
        int NEW = poll(fds, 1, 5);							
        if(NEW > 0 && fds[0].revents & POLLIN){			//якщо трапилась подія у файловому дескрипторі і це подія POLLIN
            bytes = read(fd, buff + head, sizeof(buff) - head);
            if(bytes > 0){
                head += bytes;
            }
        }
        else if(NEW == 0){
            if(head > 0){
                //processing
                char* found = (char*)memchr(buff + tail, '\n', head - tail);
                
                while(found != NULL)
                {
                    int found_pos = found - buff;
                    int sentence_len = found_pos - tail + 1; 
                    
                    char* sentence = new char[sentence_len + 1];
                    memcpy(sentence, buff + tail, sentence_len);
                    sentence[sentence_len + 1] = '\0';

                    tail = found_pos + 1;
                    nmea_base *result = Manager.Process(sentence);
                    if(result != NULL){
                        result->Show();
                        delete result;
                    }
                    delete[] sentence;
                    found = (char*)memchr(buff + tail, '\n', head - tail);
		}
            }
	    if(head >= sizeof(buff) - 100){
                head = 0;
                tail = 0;
            }
        }
    }
}
    while(1){
        // Читаємо частіше - timeout 5мс замість 50мс
        int NEW = poll(fds, 1, 5);
        if(NEW > 0 && fds[0].revents & POLLIN){
            bytes = read(fd, buff + head, sizeof(buff) - head);
            if(bytes > 0){
                head += bytes;
            }
        }
        
        // Обробка даних
        if(head > tail){
            char* found = (char*)memchr(buff + tail, '\n', head - tail);
            
            while(found != NULL)
            {
                int found_pos = found - buff;
                int sentence_len = found_pos - tail + 1;
                
                char* sentence = new char[sentence_len + 1];
                memcpy(sentence, buff + tail, sentence_len);
                sentence[sentence_len] = '\0';
                
                // Мінімальний вивід - тільки розпарсені дані
                nmea_base *result = Manager.Process(sentence);
                if(result != NULL){
                    result->Show();
                    delete result;
                }
                delete[] sentence;
                
                tail = found_pos + 1;
                found = (char*)memchr(buff + tail, '\n', head - tail);
            }
        }        
        // Захист від переповнення
        if(head >= sizeof(buff) - 100){
            head = 0;
            tail = 0;
        }
    }
