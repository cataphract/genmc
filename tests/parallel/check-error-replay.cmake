# Exercise both normal assertion replay and pool shutdown with queued revisits.
foreach(model rc11 imm)
  foreach(threads 1 2 4 16)
    foreach(trial RANGE 1 5)
      execute_process(
        COMMAND "${GENMC}" -${model} -nthreads=${threads}
                -disable-mm-detector -disable-estimation -mode=verify
                "${TEST_SOURCE}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 2
      )
      string(APPEND output "${error}")
      if(NOT "${status}" STREQUAL "42" OR
         NOT output MATCHES "Error: Safety violation!" OR
         NOT output MATCHES "ERROR error-replay.c:[0-9]+" OR
         output MATCHES "INTERNAL FAILURE")
        message(FATAL_ERROR
          "${model}, ${threads} workers, trial ${trial}: expected an assertion "
          "with its source location and exit 42; got ${status}\n${output}")
      endif()
    endforeach()
  endforeach()
endforeach()
