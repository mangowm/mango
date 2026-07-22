(define-module (mangowm)
  #:use-module (guix download)
  #:use-module (guix git-download)
  #:use-module (guix gexp)
  #:use-module (guix packages)
  #:use-module (guix utils)
  #:use-module (gnu packages window-management)
  #:use-module (gnu packages gtk)
  #:use-module (gnu packages freedesktop)
  #:use-module (gnu packages xdisorg)
  #:use-module (gnu packages pciutils)
  #:use-module (gnu packages admin)
  #:use-module (gnu packages pcre)
  #:use-module (gnu packages javascript)
  #:use-module (gnu packages xorg)
  #:use-module (gnu packages build-tools)
  #:use-module (gnu packages ninja)
  #:use-module (gnu packages pkg-config)
  #:use-module (guix build-system meson)
  #:use-module ((guix licenses) #:prefix license:))


(define-public mangowm-git
  (package
    (name "mangowm")
    (version "git")
    (source (local-file "." "mangowm-checkout"
                        #:recursive? #t
                        #:select? (or (git-predicate (current-source-directory))
                                      (const #t))))
    (build-system meson-build-system)
    (arguments
     (list
      #:configure-flags
      #~(list (string-append "-Dsysconfdir=" #$output "/etc"))
      #:phases
      #~(modify-phases %standard-phases
          (add-before 'configure 'patch-meson
            (lambda _
              (substitute* "meson.build"
                ;; MangoWM ignores sysconfdir handling for NixOS.
                ;; We also need to skip that sysconfdir edits.
                (("is_nixos = false")
                 "is_nixos = true")
                ;; Unhardcode path.  Fixes loading default config.
                (("'-DSYSCONFDIR=\\\"@0@\\\"'.format\\('/etc'\\)")
                 "'-DSYSCONFDIR=\"@0@\"'.format(sysconfdir)")))))))
    (inputs (list wayland
                  libinput
                  libdrm
                  libxkbcommon
                  pixman
                  libdisplay-info
                  libliftoff
                  hwdata
                  seatd
                  pcre2
                  pango
                  cjson
                  libxcb
                  pixman
                  xcb-util-wm
                  wlroots-0.20
                  scenefx-0.5))
    (native-inputs (list pkg-config wayland-protocols))
    (home-page "https://github.com/mangowm/mango")
    (synopsis "Wayland compositor based on wlroots and scenefx")
    (description
     "MangoWM is a modern, lightweight, high-performance Wayland compositor
built on dwl — crafted for speed, flexibility, and a customizable desktop experience.")
    (license (list license:gpl3 ;mangowm itself, dwl
                   license:expat ;dwm, sway, wlroots
                   license:cc0)))) ;tinywl

(define-public scenefx-0.5
  (package
    (inherit scenefx)
    (name "scenefx")
    (version "0.5")
    (source (origin
              (method git-fetch)
              (uri (git-reference
                     (url "https://github.com/wlrfx/scenefx")
                     (commit version)))
              (file-name (git-file-name name version))
              (sha256
               (base32
                "0klxy73125lp9jab8qghh4v6di91l3y2rgan4m4lhv5flwdwnj5x"))))
    (inputs (modify-inputs (package-inputs scenefx)
              (replace "wlroots" wlroots-0.20)))))

(define-deprecated-package mangowc
  mangowm-git)

mangowm-git
