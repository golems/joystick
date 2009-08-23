(use-package :genmsg)

(defmsgstruct js-msg
  (axes :double :dimension *)
  ;; this is a bit array...
  (buttons :int32))

(genmsgs  "js_msg.h" :c js-msg)
