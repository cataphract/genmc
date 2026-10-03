# Checks GuardSpeculativeLocalLoadsPass: the guarded-local-load* variants must
# explore their expected executions without errors, every masked-*.ll variant
# must keep its pointer-width mixed-size diagnostic, and the pass must remain
# idempotent and cheap on many loads of one alloca.

# Runs GenMC on FILE under MODEL; further arguments are passed to Clang.
function(run_genmc model file)
  execute_process(
    COMMAND "${GENMC}" "-${model}" -disable-mm-detector -disable-estimation -- ${ARGN} "${file}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 30)
  set(status "${status}" PARENT_SCOPE)
  set(output "${output}${error}" PARENT_SCOPE)
endfunction()

# Many late-guarded loads of one alloca must not recompute capture tracking
# for every intervening private read (cubic in the number of loads).
set(count 800)
set(ir "target datalayout = \"e-p:64:64-i64:64-n8:16:32:64\"
declare void @init(ptr nocapture)
declare void @use(ptr)

define void @many() {
  %s = alloca [${count} x { ptr, i8 }]
  call void @init(ptr %s)
")
math(EXPR last "${count} - 1")
foreach(i RANGE ${last})
  string(APPEND ir "  %e${i} = getelementptr [${count} x { ptr, i8 }], ptr %s, i32 0, i32 ${i}
  %f${i} = getelementptr { ptr, i8 }, ptr %e${i}, i32 0, i32 1
")
endforeach()
foreach(i RANGE ${last})
  string(APPEND ir "  %p${i} = load ptr, ptr %e${i}\n")
endforeach()
foreach(i RANGE ${last})
  string(APPEND ir "  %t${i} = load i8, ptr %f${i}\n  %h${i} = trunc i8 %t${i} to i1\n")
endforeach()
string(APPEND ir "  br label %b0\n")
foreach(i RANGE ${last})
  math(EXPR next "${i} + 1")
  string(APPEND ir "b${i}:
  %n${i} = icmp ne ptr %p${i}, null
  %d${i} = select i1 %h${i}, i1 %n${i}, i1 false
  br i1 %d${i}, label %a${i}, label %b${next}
a${i}:
  call void @use(ptr %p${i})
  br label %b${next}
")
endforeach()
string(APPEND ir "b${count}:\n  ret void\n}\n\ndefine i32 @main() {\n  ret i32 0\n}\n")
set(many "${CMAKE_CURRENT_BINARY_DIR}/guarded-local-load-many.ll")
file(WRITE "${many}" "${ir}")
execute_process(
  COMMAND "${GENMC}" -rc11 -disable-mm-detector -disable-estimation "${many}"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
if(NOT status EQUAL 0 OR NOT output MATCHES "No errors were detected")
  message(FATAL_ERROR "${count} guarded loads of one alloca failed (${status}): ${output}${error}")
endif()

# The pass runs again after jump threading has already put the load under a
# branch on its discriminator. It must not guard the load a second time.
foreach(variant expected-phi expected-late-guard)
  set(ir "${CMAKE_CURRENT_BINARY_DIR}/guarded-local-load-${variant}.ll")
  execute_process(
    COMMAND "${GENMC}" -rc11 -disable-mm-detector -disable-estimation
            "--output-llvm-after=${ir}"
            "${TEST_ROOT}/correct/infr/guarded-local-load/variants/${variant}.ll"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET TIMEOUT 30)
  file(READ "${ir}" text)
  string(REGEX MATCHALL "br i1 %has_value," guards "${text}")
  list(LENGTH guards branches)
  if(NOT status EQUAL 0 OR NOT branches EQUAL 1)
    message(FATAL_ERROR "${variant}: expected one branch on %has_value, got ${branches}:\n${text}")
  endif()
endforeach()

foreach(model rc11 imm sc)
  # Guarding must neither report errors nor change the explored executions.
  foreach(test guarded-local-load guarded-local-load-deps guarded-local-load-c)
    set(dir "${TEST_ROOT}/correct/infr/${test}")
    file(STRINGS "${dir}/expected.${model}.mo.in" expected LIMIT_COUNT 1)
    # As in runcorrect.sh, an args file holds "genmc args | clang args".
    set(clang_args "")
    if(EXISTS "${dir}/args.${model}.mo.in")
      file(STRINGS "${dir}/args.${model}.mo.in" args LIMIT_COUNT 1)
      string(REGEX REPLACE "^[^|]*\\|" "" args "${args}")
      separate_arguments(clang_args UNIX_COMMAND "${args}")
    endif()
    file(GLOB variants "${dir}/variants/*.ll" "${dir}/variants/*.c")
    foreach(variant IN LISTS variants)
      run_genmc(${model} "${variant}" ${clang_args})
      string(REGEX MATCH "complete executions explored: ([0-9]+)" explored "${output}")
      set(explored "${CMAKE_MATCH_1}")
      if(NOT status EQUAL 0 OR NOT output MATCHES "No errors were detected" OR
         NOT explored STREQUAL expected)
        message(FATAL_ERROR
          "${variant} failed under ${model} (expected ${expected} executions): ${output}")
      endif()
    endforeach()
  endforeach()

  # runwrong.sh only runs C/C++ variants, so this is the sole runner of these.
  # Each must report its pointer-width speculative load: a later, unrelated
  # mixed-size access (e.g., of a second call) must not stand in for it.
  file(GLOB variants "${TEST_ROOT}/wrong/infr/mixed-size-na/variants/masked-*.ll")
  foreach(variant IN LISTS variants)
    run_genmc(${model} "${variant}")
    if(NOT status EQUAL 42 OR NOT output MATCHES "Error: Mixed-size accesses!" OR
       NOT output MATCHES "tried to read with a 64-bit access!")
      message(FATAL_ERROR "Lost ${variant} diagnostic under ${model}: ${output}")
    endif()
  endforeach()
endforeach()
