# wrw/4 generates enough revisits to move cloned graphs between workers.
# IMM prefixes contain DepViews whose intrusive ViewBase must be detached.
# Compare every parallel run with a serial run to catch corruption as well
# as crashes, and also exercise the ordinary View prefixes used by RC11.
foreach(model rc11 imm)
  foreach(threads 1 2 16)
    if(threads EQUAL 1)
      set(trials 1)
    else()
      set(trials 10)
    endif()
    foreach(trial RANGE 1 ${trials})
      execute_process(
        COMMAND "${GENMC}" -${model} -nthreads=${threads}
                -disable-mm-detector -disable-estimation -disable-ipr -disable-sr
                -- -DN=4 "${TEST_SOURCE}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 5
      )
      string(APPEND output "${error}")
      if(NOT "${status}" STREQUAL "0" OR
         NOT output MATCHES "No errors were detected." OR
         NOT output MATCHES "Number of complete executions explored: ([0-9]+)")
        message(FATAL_ERROR
          "${model}, ${threads} workers, trial ${trial}: verification failed "
          "with ${status}\n${output}")
      endif()
      set(executions "${CMAKE_MATCH_1}")
      if(threads EQUAL 1)
        set(expected "${executions}")
      elseif(NOT executions STREQUAL expected)
        message(FATAL_ERROR
          "${model}, ${threads} workers, trial ${trial}: expected ${expected} "
          "executions, got ${executions}\n${output}")
      endif()
    endforeach()
  endforeach()
endforeach()
