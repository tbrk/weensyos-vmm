BEGIN { 
    print "digraph G {" 
    print "  rankdir=LR;"
    print "  node [shape=box];"
}

{
    target = $NF                     # Store the last field (the target)
    for (i = 2; i <= NF - 2; i++) {  # Loop from the first .o file to the last
        print "  \"" $i "\" -> \"" target "\";"
    }
}

END {
    print "}"
}

