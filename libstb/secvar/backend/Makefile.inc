# SPDX-License-Identifier: Apache-2.0
# -*-Makefile-*-

SECVAR_BACKEND_DIR = libstb/secvar/backend

SUBDIRS += $(SECVAR_BACKEND_DIR)

SECVAR_BACKEND_SRCS =
SECVAR_BACKEND_OBJS = $(SECVAR_BACKEND_SRCS:%.c=%.o)
SECVAR_BACKEND = $(SECVAR_BACKEND_DIR)/built-in.a

$(SECVAR_BACKEND): $(SECVAR_BACKEND_OBJS:%=$(SECVAR_BACKEND_DIR)/%)