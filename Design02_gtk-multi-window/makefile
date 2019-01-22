SRC		= main.c
OUT		= a.out
RUNCMD		= ./a.out
GREEN		= \033[0;32m
NC		= \033[0m
PREFINFO	= $(GREEN)💬$(NC)

main: $(OUT)
	@echo "$(PREFINFO) Start running: $(RUNCMD)"
	@$(RUNCMD)
	@echo "$(PREFINFO) Done!"

$(OUT): $(SRC)
	@echo "$(PREFINFO) Source out-dated, re-buliding..."
	@gcc `pkg-config --cflags gtk+-3.0` -o $(OUT) $(SRC) `pkg-config --libs gtk+-3.0`

