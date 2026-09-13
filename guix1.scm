;;; Guix package definition for TPCC, the C++20-emitting Pascal compiler.
;;;
;;; Build:   guix build -f guix1.scm
;;; Install: guix package -f guix1.scm

(use-modules (guix packages)
             (guix git-download)
             (guix gexp)
             (guix build-system gnu)
             ((guix licenses) #:prefix license:))

(define %source-directory
  (dirname (current-filename)))

(define %git-file?
  (git-predicate %source-directory))

;; The libstdc++ shipped with the Guix gcc-16.2.0 toolchain leaves both
;; _GLIBCXX_HAVE_FENV_H and _GLIBCXX_USE_C99_FENV undefined in its generated
;; c++config.h.  libstdc++'s <fenv.h> therefore never #include_next's glibc's
;; and FE_ALL_EXCEPT, feenableexcept, fegetexcept &c. disappear.  rtl/rtl.h
;; includes <fenv.h> and uses those names, so every C++ translation unit that
;; includes the runtime (all generated code) fails to compile.  Two fixes work
;; and neither touches the repository:
;;
;;   g++ -D_GLIBCXX_HAVE_FENV_H=1
;;   CPLUS_INCLUDE_PATH="$prefix/include:$prefix/include/c++"   # glibc's fenv.h wins
;;
;; The -D form is used here and is passed through CXX rather than CXXFLAGS so
;; the Makefile's own flags stay in one place.
(define %fenv-workaround "-D_GLIBCXX_HAVE_FENV_H=1")

(package
  (name "tpcc")
  (version "0.1.0")
  (source (local-file %source-directory
                      "tpcc-checkout"
                      #:recursive? #t
                      #:select?
                      (lambda (file stat)
                        (and (or (not %git-file?)
                                 (%git-file? file stat))
                             ;; Never hash this packaging file into the source
                             ;; it describes.
                             (not (string-suffix? ".scm" file))))))
  (build-system gnu-build-system)
  (arguments
   (list
    #:make-flags
    #~(list (string-append "PREFIX=" #$output)
            (string-append "CXX=g++ " #$%fenv-workaround))
    #:tests? #f ; run separately
    #:phases
    #~(modify-phases %standard-phases
        (delete 'configure)))) ; no configure script
  (home-page "https://github.com/daym/tpcc")
  (synopsis "Small Pascal compiler that emits C++20")
  (description
   "TPCC is a deliberately straightforward Pascal compiler.  It emits C++20
and only targets C++ compilers where strict aliasing can be disabled.  It
assumes a GObject-style ABI in which object and data pointers share a
representation and argument slots.")
  ;; TODO: the tree has no LICENSE file yet.  Placeholder until one lands.
  (license #f))
