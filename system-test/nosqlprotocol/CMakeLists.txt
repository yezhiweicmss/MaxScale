# Copy files to binary directory
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/package.json ${CMAKE_CURRENT_BINARY_DIR}/package-lock.json
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package.json ${CMAKE_CURRENT_SOURCE_DIR}/package-lock.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/package.json ${CMAKE_CURRENT_SOURCE_DIR}/package-lock.json .)
add_custom_target(copy_npm_files ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/package.json ${CMAKE_CURRENT_BINARY_DIR}/package-lock.json)

# Run npm install
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/node_modules
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/package.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND npm install)
add_custom_target(install_node_modules ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/node_modules)
add_dependencies(install_node_modules copy_npm_files)

# Copy test files to binary directory
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND cp -R ${CMAKE_CURRENT_SOURCE_DIR}/test .)
add_custom_target(copy_test_files ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test)
add_dependencies(copy_test_files install_node_modules)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/compare
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/compare
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND cp -R ${CMAKE_CURRENT_SOURCE_DIR}/compare .)
add_custom_target(copy_compare_files ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/compare)
add_dependencies(copy_compare_files copy_test_files)

#
# Tests
#
add_test_script(nosqlprotocol nosqlprotocol.sh nosqlprotocol LABELS nosqlprotocol REPL_BACKEND)
