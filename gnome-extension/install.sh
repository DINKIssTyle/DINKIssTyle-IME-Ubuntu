#!/bin/bash
# Created by DINKIssTyle on 2026. Copyright (C) 2026 DINKI'ssTyle. All rights reserved.
#
# DKST IME Indicator - GNOME Shell Extension Installer
# Installs/uninstalls the extension to the user's GNOME Shell extensions directory.

set -euo pipefail

# ── Constants ──
EXTENSION_UUID="dkst-indicator@dinkisstyle.com"
EXTENSION_NAME="DKST IME Indicator"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST_DIR="$HOME/.local/share/gnome-shell/extensions/$EXTENSION_UUID"

# ── Colors ──
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m' # No Color

# ── Utility Functions ──

print_banner() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}  ${BOLD}DKST IME Indicator${NC} — GNOME Shell Extension  ${CYAN}║${NC}"
    echo -e "${CYAN}║${NC}  ${DIM}한/영 상태 아이콘 표시 확장 프로그램${NC}         ${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════╝${NC}"
    echo ""
}

print_step() {
    echo -e "  ${BLUE}▸${NC} $1"
}

print_success() {
    echo -e "  ${GREEN}✓${NC} $1"
}

print_warn() {
    echo -e "  ${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "  ${RED}✗${NC} $1"
}

print_info() {
    echo -e "  ${DIM}$1${NC}"
}

# Check if extension is currently installed
is_installed() {
    [ -d "$DEST_DIR" ] && [ -f "$DEST_DIR/metadata.json" ]
}

# Check if extension is currently enabled
is_enabled() {
    if command -v gnome-extensions &>/dev/null; then
        gnome-extensions list --enabled 2>/dev/null | grep -q "$EXTENSION_UUID"
    else
        return 1
    fi
}

# Get current GNOME Shell version
get_gnome_version() {
    gnome-shell --version 2>/dev/null | grep -oP '[\d.]+' || echo "알 수 없음"
}

# Show current status
show_status() {
    echo -e "${BOLD}현재 상태:${NC}"
    
    local gnome_ver
    gnome_ver=$(get_gnome_version)
    print_info "GNOME Shell 버전: ${gnome_ver}"
    
    if is_installed; then
        print_success "설치됨: ${DIM}${DEST_DIR}${NC}"
    else
        print_info "설치되지 않음"
    fi
    
    if is_enabled; then
        print_success "확장 활성화됨"
    else
        print_info "확장 비활성화됨"
    fi
    echo ""
}

# ── Install ──

install_extension() {
    echo ""
    echo -e "${BOLD}── 설치 진행 ──${NC}"
    echo ""

    # Check if already installed
    if is_installed; then
        print_warn "이미 설치되어 있습니다. 덮어쓰기합니다."
        echo ""
    fi

    # Step 1: Verify source files
    print_step "소스 파일 확인 중..."
    
    local missing=0
    for f in metadata.json extension.js stylesheet.css; do
        if [ ! -f "$SCRIPT_DIR/$f" ]; then
            print_error "누락된 파일: $f"
            missing=1
        fi
    done

    if [ ! -d "$SCRIPT_DIR/icons" ] || [ -z "$(ls "$SCRIPT_DIR/icons/"*.svg 2>/dev/null)" ]; then
        print_error "누락된 디렉토리 또는 아이콘: icons/*.svg"
        missing=1
    fi

    if [ "$missing" -eq 1 ]; then
        echo ""
        print_error "필수 파일이 누락되어 설치를 진행할 수 없습니다."
        exit 1
    fi
    print_success "소스 파일 확인 완료"

    # Step 2: Create destination directory
    print_step "설치 디렉토리 생성 중..."
    mkdir -p "$DEST_DIR/icons"
    print_success "디렉토리 생성: ${DIM}${DEST_DIR}${NC}"

    # Step 3: Copy files
    print_step "파일 복사 중..."
    cp "$SCRIPT_DIR/metadata.json" "$DEST_DIR/"
    cp "$SCRIPT_DIR/extension.js"  "$DEST_DIR/"
    cp "$SCRIPT_DIR/stylesheet.css" "$DEST_DIR/"
    cp "$SCRIPT_DIR/icons/"*.svg    "$DEST_DIR/icons/"
    
    # Count copied files
    local count
    count=$(find "$DEST_DIR" -type f | wc -l)
    print_success "${count}개 파일 복사 완료"

    # Step 4: Enable the extension
    print_step "확장 활성화 중..."
    if command -v gnome-extensions &>/dev/null; then
        if gnome-extensions enable "$EXTENSION_UUID" 2>/dev/null; then
            print_success "확장 활성화 완료"
        else
            print_warn "자동 활성화 실패 (수동 활성화 필요)"
        fi
    else
        print_warn "gnome-extensions 명령어를 찾을 수 없습니다"
    fi

    # Done!
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${NC}  ${BOLD}${GREEN}✓ 설치가 완료되었습니다!${NC}            ${GREEN}║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${BOLD}적용하려면 다음 중 하나를 수행하세요:${NC}"
    echo ""
    echo -e "  ${CYAN}1.${NC} 로그아웃 후 다시 로그인"
    echo -e "  ${CYAN}2.${NC} Alt+F2 → ${BOLD}r${NC} 입력 ${DIM}(X11에서만 가능)${NC}"
    echo -e "  ${CYAN}3.${NC} ${DIM}gnome-extensions enable ${EXTENSION_UUID}${NC}"
    echo ""
}

# ── Uninstall ──

uninstall_extension() {
    echo ""
    echo -e "${BOLD}── 제거 진행 ──${NC}"
    echo ""

    # Check if installed
    if ! is_installed; then
        print_warn "확장이 설치되어 있지 않습니다."
        print_info "경로: ${DEST_DIR}"
        echo ""
        return 0
    fi

    # Step 1: Disable the extension
    print_step "확장 비활성화 중..."
    if command -v gnome-extensions &>/dev/null; then
        if gnome-extensions disable "$EXTENSION_UUID" 2>/dev/null; then
            print_success "확장 비활성화 완료"
        else
            print_warn "이미 비활성화 상태이거나 비활성화 실패"
        fi
    else
        print_warn "gnome-extensions 명령어를 찾을 수 없습니다"
    fi

    # Step 2: Remove files
    print_step "파일 제거 중..."
    
    local count
    count=$(find "$DEST_DIR" -type f | wc -l)
    rm -rf "$DEST_DIR"
    print_success "${count}개 파일 제거 완료"
    print_info "제거됨: ${DEST_DIR}"

    # Done!
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${NC}  ${BOLD}${GREEN}✓ 제거가 완료되었습니다!${NC}            ${GREEN}║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  ${DIM}변경사항을 적용하려면 로그아웃 후 다시 로그인하세요.${NC}"
    echo ""
}

# ── Confirm Prompt ──

confirm() {
    local message="$1"
    local default="${2:-y}"
    
    local prompt
    if [ "$default" = "y" ]; then
        prompt="[Y/n]"
    else
        prompt="[y/N]"
    fi
    
    echo -ne "  ${YELLOW}?${NC} ${message} ${DIM}${prompt}${NC} "
    read -r answer
    
    if [ -z "$answer" ]; then
        answer="$default"
    fi
    
    case "$answer" in
        [Yy]*) return 0 ;;
        *) return 1 ;;
    esac
}

# ── Interactive Menu ──

show_menu() {
    print_banner
    show_status

    echo -e "${BOLD}무엇을 하시겠습니까?${NC}"
    echo ""
    echo -e "  ${CYAN}1)${NC}  설치  ${DIM}— 확장 프로그램을 설치합니다${NC}"
    echo -e "  ${CYAN}2)${NC}  제거  ${DIM}— 확장 프로그램을 제거합니다${NC}"
    echo -e "  ${CYAN}q)${NC}  종료"
    echo ""
    echo -ne "  선택 ${DIM}[1/2/q]:${NC} "
    read -r choice

    case "$choice" in
        1|install|설치)
            echo ""
            if confirm "확장 프로그램을 설치하시겠습니까?"; then
                install_extension
            else
                echo ""
                print_info "설치가 취소되었습니다."
                echo ""
            fi
            ;;
        2|uninstall|remove|제거)
            echo ""
            if confirm "확장 프로그램을 제거하시겠습니까?" "n"; then
                uninstall_extension
            else
                echo ""
                print_info "제거가 취소되었습니다."
                echo ""
            fi
            ;;
        q|Q|quit|exit|종료)
            echo ""
            print_info "종료합니다."
            echo ""
            ;;
        *)
            echo ""
            print_error "잘못된 선택입니다. 1, 2, 또는 q를 입력하세요."
            echo ""
            ;;
    esac
}

# ── Entry Point ──

# If arguments are provided, run in non-interactive mode (backward compatible)
if [ $# -gt 0 ]; then
    print_banner
    case "$1" in
        install)
            install_extension
            ;;
        uninstall|remove)
            uninstall_extension
            ;;
        status)
            show_status
            ;;
        *)
            echo -e "사용법: $0 ${DIM}[install|uninstall|status]${NC}"
            echo ""
            echo -e "  ${CYAN}install${NC}     확장 프로그램 설치"
            echo -e "  ${CYAN}uninstall${NC}   확장 프로그램 제거"
            echo -e "  ${CYAN}status${NC}      현재 상태 확인"
            echo ""
            echo -e "  ${DIM}인자 없이 실행하면 대화형 메뉴가 표시됩니다.${NC}"
            exit 1
            ;;
    esac
else
    # No arguments: show interactive menu
    show_menu
fi
