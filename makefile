GREEN = \033[0;32m
BLUE = \033[0;34m
RED = \033[0;31m
NC = \033[0m

all: compile

compile: main.cpp
	@echo "${BLUE}started to compiling${NC}"
	@mkdir -p build
	@echo "${GREEN}created `build` folder${NC}"
	@g++ main.cpp -o build/oditer `pkg-config --cflags --libs gtkmm-3.0`
	@echo "${GREEN}compilation successful${NC}"

clean:
	@echo "${RED}cleaning up${NC}"
	@rm -rf build
	@echo "${RED}`build` folder cleaned up${NC}"

run: compile
	@./build/oditer