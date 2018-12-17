;;; -------- David mode --------

(define-derived-mode
  david-mode text-mode
  "David" "View mode for David's input file."
  (setq david-font-lock-keywords
	'(("#.*$" . font-lock-comment-face)
	  ("\\(\"[^\"]*\"\\)\\s-*[,(){}]" 1 font-lock-string-face)
	  (" \\^ " . font-lock-keyword-face)
	  (" => "  . font-lock-keyword-face)
	  ("[(,]\\s-*\\(\\*\\)[a-z]" 1 font-lock-warning-face)
	  ("\\<not\\>" . font-lock-warning-face)
	  ("!"         . font-lock-warning-face)
	  ("^\\(problem\\|rule\\|property\\|mutual-exclusion\\)\\s-*{" 1 font-lock-type-face)
	  ("^\\(problem\\|rule\\|property\\|mutual-exclusion\\)\\s-+\\([^ \t\n{]+\\)\\s-*{"
	   (1 font-lock-type-face)
	   (2 font-lock-function-name-face))
	  ("\\<\\(observe\\|query\\|fact\\|require\\)\\s-*{" 1 font-lock-constant-face)
	  ("\\<\\(transitive\\|asymmetric\\|symmetric\\|irreflexive\\|right-unique\\|left-unique\\|closed\\|abstract\\):"
	   (1 font-lock-constant-face))
	  (":[^ \t\n(){}]+" . font-lock-doc-face)))
  (setq font-lock-defaults
	  '(david-font-lock-keywords t t nil nil))
  (add-hook 'david-mode-hook 'turn-on-font-lock))

(add-to-list 'auto-mode-alist '("\\.dav$" . david-mode))
