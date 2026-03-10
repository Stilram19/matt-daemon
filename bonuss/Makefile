NAME        := Matt_daemon
CLIENT      := Ben_AFK

SRCS = \
	src/client.cpp  \
	src/aes.cpp \
	src/main.cpp \
	src/Matt_daemon.cpp \
	src/session_key.cpp \
	src/Shell.cpp \
	src/Tintin_reporter.cpp

SERVER_SRCS = \
	src/aes.cpp \
	src/main.cpp \
	src/Matt_daemon.cpp \
	src/session_key.cpp \
	src/Shell.cpp \
	src/Tintin_reporter.cpp

CLIENT_SRCS = \
	src/client.cpp \
	src/aes.cpp



#-Wall -Wextra -Werror
CXXFLAGS = -Wall -Wextra -Werror
INCLUDE = -I ./include/ 
LINKING = -lssl -lcrypto
# -fsanitize=address


OBJ_DIR = _OUT/
OBJ = $(addprefix $(OBJ_DIR), $(SRCS:.cpp=.o))
CLIENT_OBJ = $(addprefix $(OBJ_DIR), $(CLIENT_SRCS:.cpp=.o))
SERVER_OBJ = $(addprefix $(OBJ_DIR), $(SERVER_SRCS:.cpp=.o))

DEPFLAGS = -MMD -MF $(@:.o=.d)
deps = $(OBJ:.o=.d)
deps = $(CLIENT_OBJ:.o=.d)

all : $(NAME) $(CLIENT)

$(NAME) : $(SERVER_OBJ)
	$(CXX) $(SERVER_OBJ) $(LINKING) -o $(NAME)

$(CLIENT): $(CLIENT_OBJ)
	$(CXX) $(CLIENT_OBJ) $(LINKING) -o $(CLIENT)

$(OBJ) : $(OBJ_DIR)%.o : %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< $(DEPFLAGS) -o $@


-include $(deps)

gui_client:
	make -C ..
	mv ../Matt_daemon .
	${CXX} src/gui_client.cpp -o ${CLIENT} ${CXXFLAGS}  `pkg-config gtkmm-3.0 --cflags --libs` -pthread

clean :
	rm -rf $(OBJ_DIR)

fclean : clean
	rm -f $(NAME) $(CLIENT)

re : fclean all