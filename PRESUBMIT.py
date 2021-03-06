# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import json
import os
import platform
import re
import subprocess
import sys


# Directories that will be scanned by cpplint by the presubmit script.
CPPLINT_DIRS = [
  'webrtc/audio',
  'webrtc/call',
  'webrtc/common_video',
  'webrtc/examples',
  'webrtc/modules/bitrate_controller',
  'webrtc/modules/congestion_controller',
  'webrtc/modules/pacing',
  'webrtc/modules/remote_bitrate_estimator',
  'webrtc/modules/rtp_rtcp',
  'webrtc/modules/video_coding',
  'webrtc/modules/video_processing',
  'webrtc/tools',
  'webrtc/video',
]

# These filters will always be removed, even if the caller specifies a filter
# set, as they are problematic or broken in some way.
#
# Justifications for each filter:
# - build/c++11         : Rvalue ref checks are unreliable (false positives),
#                         include file and feature blacklists are
#                         google3-specific.
# - whitespace/operators: Same as above (doesn't seem sufficient to eliminate
#                         all move-related errors).
BLACKLIST_LINT_FILTERS = [
  '-build/c++11',
  '-whitespace/operators',
]

# List of directories of "supported" native APIs. That means changes to headers
# will be done in a compatible way following this scheme:
# 1. Non-breaking changes are made.
# 2. The old APIs as marked as deprecated (with comments).
# 3. Deprecation is announced to discuss-webrtc@googlegroups.com and
#    webrtc-users@google.com (internal list).
# 4. (later) The deprecated APIs are removed.
NATIVE_API_DIRS = (
  'webrtc',
  'webrtc/api',
  'webrtc/media',
  'webrtc/modules/audio_device/include',
  'webrtc/pc',
)
# These directories should not be used but are maintained only to avoid breaking
# some legacy downstream code.
LEGACY_API_DIRS = (
  'talk/app/webrtc',
  'webrtc/base',
  'webrtc/common_audio/include',
  'webrtc/modules/audio_coding/include',
  'webrtc/modules/audio_conference_mixer/include',
  'webrtc/modules/audio_processing/include',
  'webrtc/modules/bitrate_controller/include',
  'webrtc/modules/congestion_controller/include',
  'webrtc/modules/include',
  'webrtc/modules/remote_bitrate_estimator/include',
  'webrtc/modules/rtp_rtcp/include',
  'webrtc/modules/rtp_rtcp/source',
  'webrtc/modules/utility/include',
  'webrtc/modules/video_coding/codecs/h264/include',
  'webrtc/modules/video_coding/codecs/i420/include',
  'webrtc/modules/video_coding/codecs/vp8/include',
  'webrtc/modules/video_coding/codecs/vp9/include',
  'webrtc/modules/video_coding/include',
  'webrtc/system_wrappers/include',
  'webrtc/voice_engine/include',
)
API_DIRS = NATIVE_API_DIRS[:] + LEGACY_API_DIRS[:]


def _VerifyNativeApiHeadersListIsValid(input_api, output_api):
  """Ensures the list of native API header directories is up to date."""
  non_existing_paths = []
  native_api_full_paths = [
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             *path.split('/')) for path in API_DIRS]
  for path in native_api_full_paths:
    if not os.path.isdir(path):
      non_existing_paths.append(path)
  if non_existing_paths:
    return [output_api.PresubmitError(
        'Directories to native API headers have changed which has made the '
        'list in PRESUBMIT.py outdated.\nPlease update it to the current '
        'location of our native APIs.',
        non_existing_paths)]
  return []

api_change_msg = """
You seem to be changing native API header files. Please make sure that you:
  1. Make compatible changes that don't break existing clients.
  2. Mark the old stuff as deprecated.
  3. Create a timeline and plan for when the deprecated stuff will be
     removed. (The amount of time we give users to change their code
     should be informed by how much work it is for them. If they just
     need to replace one name with another or something equally
     simple, 1-2 weeks might be good; if they need to do serious work,
     up to 3 months may be called for.)
  4. Update/inform existing downstream code owners to stop using the
     deprecated stuff. (Send announcements to
     discuss-webrtc@googlegroups.com and webrtc-users@google.com.)
  5. Remove the deprecated stuff, once the agreed-upon amount of time
     has passed.
Related files:
"""

def _CheckNativeApiHeaderChanges(input_api, output_api):
  """Checks to remind proper changing of native APIs."""
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if f.LocalPath().endswith('.h'):
      for path in API_DIRS:
        if os.path.dirname(f.LocalPath()) == path:
          files.append(f)

  if files:
    return [output_api.PresubmitNotifyResult(api_change_msg, files)]
  return []


def _CheckNoIOStreamInHeaders(input_api, output_api):
  """Checks to make sure no .h files include <iostream>."""
  files = []
  pattern = input_api.re.compile(r'^#include\s*<iostream>',
                                 input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not f.LocalPath().endswith('.h'):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if len(files):
    return [output_api.PresubmitError(
        'Do not #include <iostream> in header files, since it inserts static ' +
        'initialization into every file including the header. Instead, ' +
        '#include <ostream>. See http://crbug.com/94794',
        files)]
  return []


def _CheckNoFRIEND_TEST(input_api, output_api):
  """Make sure that gtest's FRIEND_TEST() macro is not used, the
  FRIEND_TEST_ALL_PREFIXES() macro from testsupport/gtest_prod_util.h should be
  used instead since that allows for FLAKY_, FAILS_ and DISABLED_ prefixes."""
  problems = []

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      if 'FRIEND_TEST(' in line:
        problems.append('    %s:%d' % (f.LocalPath(), line_num))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('WebRTC\'s code should not use '
      'gtest\'s FRIEND_TEST() macro. Include testsupport/gtest_prod_util.h and '
      'use FRIEND_TEST_ALL_PREFIXES() instead.\n' + '\n'.join(problems))]


def _IsLintWhitelisted(whitelist_dirs, file_path):
  """ Checks if a file is whitelisted for lint check."""
  for path in whitelist_dirs:
    if os.path.dirname(file_path).startswith(path):
      return True
  return False


def _CheckApprovedFilesLintClean(input_api, output_api,
                                 source_file_filter=None):
  """Checks that all new or whitelisted .cc and .h files pass cpplint.py.
  This check is based on _CheckChangeLintsClean in
  depot_tools/presubmit_canned_checks.py but has less filters and only checks
  added files."""
  result = []

  # Initialize cpplint.
  import cpplint
  # Access to a protected member _XX of a client class
  # pylint: disable=W0212
  cpplint._cpplint_state.ResetErrorCounts()

  lint_filters = cpplint._Filters()
  lint_filters.extend(BLACKLIST_LINT_FILTERS)
  cpplint._SetFilters(','.join(lint_filters))

  # Create a platform independent whitelist for the CPPLINT_DIRS.
  whitelist_dirs = [input_api.os_path.join(*path.split('/'))
                    for path in CPPLINT_DIRS]

  # Use the strictest verbosity level for cpplint.py (level 1) which is the
  # default when running cpplint.py from command line.
  # To make it possible to work with not-yet-converted code, we're only applying
  # it to new (or moved/renamed) files and files listed in LINT_FOLDERS.
  verbosity_level = 1
  files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    # Note that moved/renamed files also count as added.
    if f.Action() == 'A' or _IsLintWhitelisted(whitelist_dirs, f.LocalPath()):
      files.append(f.AbsoluteLocalPath())

  for file_name in files:
    cpplint.ProcessFile(file_name, verbosity_level)

  if cpplint._cpplint_state.error_count > 0:
    if input_api.is_committing:
      # TODO(kjellander): Change back to PresubmitError below when we're
      # confident with the lint settings.
      res_type = output_api.PresubmitPromptWarning
    else:
      res_type = output_api.PresubmitPromptWarning
    result = [res_type('Changelist failed cpplint.py check.')]

  return result

def _CheckNoRtcBaseDeps(input_api, gyp_files, output_api):
  pattern = input_api.re.compile(r"base.gyp:rtc_base\s*'")
  violating_files = []
  for f in gyp_files:
    gyp_exceptions = (
        'base_tests.gyp',
        'desktop_capture.gypi',
        'p2p.gyp',
        'sdk.gyp',
        'webrtc_test_common.gyp',
        'webrtc_tests.gypi',
    )
    if f.LocalPath().endswith(gyp_exceptions):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      violating_files.append(f)
  if violating_files:
    return [output_api.PresubmitError(
        'Depending on rtc_base is not allowed. Change your dependency to '
        'rtc_base_approved and possibly sanitize and move the desired source '
        'file(s) to rtc_base_approved.\nChanged GYP files:',
        items=violating_files)]
  return []

def _CheckNoSourcesAboveGyp(input_api, gyp_files, output_api):
  # Disallow referencing source files with paths above the GYP file location.
  source_pattern = input_api.re.compile(r'\'sources\'.*?\[(.*?)\]',
                                        re.MULTILINE | re.DOTALL)
  file_pattern = input_api.re.compile(r"'((\.\./.*?)|(<\(webrtc_root\).*?))'")
  violating_gyp_files = set()
  violating_source_entries = []
  for gyp_file in gyp_files:
    if 'supplement.gypi' in gyp_file.LocalPath():
      # Exclude supplement.gypi from this check, as the LSan and TSan
      # suppression files are located in a different location.
      continue
    contents = input_api.ReadFile(gyp_file)
    for source_block_match in source_pattern.finditer(contents):
      # Find all source list entries starting with ../ in the source block
      # (exclude overrides entries).
      for file_list_match in file_pattern.finditer(source_block_match.group(1)):
        source_file = file_list_match.group(1)
        if 'overrides/' not in source_file:
          violating_source_entries.append(source_file)
          violating_gyp_files.add(gyp_file)
  if violating_gyp_files:
    return [output_api.PresubmitError(
        'Referencing source files above the directory of the GYP file is not '
        'allowed. Please introduce new GYP targets and/or GYP files in the '
        'proper location instead.\n'
        'Invalid source entries:\n'
        '%s\n'
        'Violating GYP files:' % '\n'.join(violating_source_entries),
        items=violating_gyp_files)]
  return []

def _CheckGypChanges(input_api, output_api):
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x, white_list=(r'.+\.(gyp|gypi)$',))

  gyp_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    if f.LocalPath().startswith('webrtc'):
      gyp_files.append(f)

  result = []
  if gyp_files:
    result.append(output_api.PresubmitNotifyResult(
        'As you\'re changing GYP files: please make sure corresponding '
        'BUILD.gn files are also updated.\nChanged GYP files:',
        items=gyp_files))
    result.extend(_CheckNoRtcBaseDeps(input_api, gyp_files, output_api))
    result.extend(_CheckNoSourcesAboveGyp(input_api, gyp_files, output_api))
  return result

def _CheckUnwantedDependencies(input_api, output_api):
  """Runs checkdeps on #include statements added in this
  change. Breaking - rules is an error, breaking ! rules is a
  warning.
  """
  # Copied from Chromium's src/PRESUBMIT.py.

  # We need to wait until we have an input_api object and use this
  # roundabout construct to import checkdeps because this file is
  # eval-ed and thus doesn't have __file__.
  original_sys_path = sys.path
  try:
    checkdeps_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                            'buildtools', 'checkdeps')
    if not os.path.exists(checkdeps_path):
      return [output_api.PresubmitError(
          'Cannot find checkdeps at %s\nHave you run "gclient sync" to '
          'download Chromium and setup the symlinks?' % checkdeps_path)]
    sys.path.append(checkdeps_path)
    import checkdeps
    from cpp_checker import CppChecker
    from rules import Rule
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  added_includes = []
  for f in input_api.AffectedFiles():
    if not CppChecker.IsCppFile(f.LocalPath()):
      continue

    changed_lines = [line for _, line in f.ChangedContents()]
    added_includes.append([f.LocalPath(), changed_lines])

  deps_checker = checkdeps.DepsChecker(input_api.PresubmitLocalPath())

  error_descriptions = []
  warning_descriptions = []
  for path, rule_type, rule_description in deps_checker.CheckAddedCppIncludes(
      added_includes):
    description_with_path = '%s\n    %s' % (path, rule_description)
    if rule_type == Rule.DISALLOW:
      error_descriptions.append(description_with_path)
    else:
      warning_descriptions.append(description_with_path)

  results = []
  if error_descriptions:
    results.append(output_api.PresubmitError(
        'You added one or more #includes that violate checkdeps rules.',
        error_descriptions))
  if warning_descriptions:
    results.append(output_api.PresubmitPromptOrNotify(
        'You added one or more #includes of files that are temporarily\n'
        'allowed but being removed. Can you avoid introducing the\n'
        '#include? See relevant DEPS file(s) for details and contacts.',
        warning_descriptions))
  return results


def _CheckJSONParseErrors(input_api, output_api):
  """Check that JSON files do not contain syntax errors."""

  def FilterFile(affected_file):
    return input_api.os_path.splitext(affected_file.LocalPath())[1] == '.json'

  def GetJSONParseError(input_api, filename):
    try:
      contents = input_api.ReadFile(filename)
      input_api.json.loads(contents)
    except ValueError as e:
      return e
    return None

  results = []
  for affected_file in input_api.AffectedFiles(
      file_filter=FilterFile, include_deletes=False):
    parse_error = GetJSONParseError(input_api,
                                    affected_file.AbsoluteLocalPath())
    if parse_error:
      results.append(output_api.PresubmitError('%s could not be parsed: %s' %
          (affected_file.LocalPath(), parse_error)))
  return results


def _RunPythonTests(input_api, output_api):
  def join(*args):
    return input_api.os_path.join(input_api.PresubmitLocalPath(), *args)

  test_directories = [
    join('tools', 'autoroller', 'unittests'),
    join('webrtc', 'tools', 'py_event_log_analyzer'),
  ]

  tests = []
  for directory in test_directories:
    tests.extend(
      input_api.canned_checks.GetUnitTestsInDirectory(
          input_api,
          output_api,
          directory,
          whitelist=[r'.+_test\.py$']))
  return input_api.RunTests(tests, parallel=True)


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  # Filter out files that are in objc or ios dirs from being cpplint-ed since
  # they do not follow C++ lint rules.
  black_list = input_api.DEFAULT_BLACK_LIST + (
    r".*\bobjc[\\\/].*",
  )
  source_file_filter = lambda x: input_api.FilterSourceFile(x, None, black_list)
  results.extend(_CheckApprovedFilesLintClean(
      input_api, output_api, source_file_filter))
  results.extend(input_api.canned_checks.RunPylint(input_api, output_api,
      black_list=(r'^.*gviz_api\.py$',
                  r'^.*gaeunit\.py$',
                  # Embedded shell-script fakes out pylint.
                  r'^build[\\\/].*\.py$',
                  r'^buildtools[\\\/].*\.py$',
                  r'^chromium[\\\/].*\.py$',
                  r'^mojo.*[\\\/].*\.py$',
                  r'^out.*[\\\/].*\.py$',
                  r'^testing[\\\/].*\.py$',
                  r'^third_party[\\\/].*\.py$',
                  r'^tools[\\\/]clang[\\\/].*\.py$',
                  r'^tools[\\\/]generate_library_loader[\\\/].*\.py$',
                  r'^tools[\\\/]generate_stubs[\\\/].*\.py$',
                  r'^tools[\\\/]gn[\\\/].*\.py$',
                  r'^tools[\\\/]gyp[\\\/].*\.py$',
                  r'^tools[\\\/]isolate_driver.py$',
                  r'^tools[\\\/]mb[\\\/].*\.py$',
                  r'^tools[\\\/]protoc_wrapper[\\\/].*\.py$',
                  r'^tools[\\\/]python[\\\/].*\.py$',
                  r'^tools[\\\/]python_charts[\\\/]data[\\\/].*\.py$',
                  r'^tools[\\\/]refactoring[\\\/].*\.py$',
                  r'^tools[\\\/]swarming_client[\\\/].*\.py$',
                  r'^tools[\\\/]vim[\\\/].*\.py$',
                  # TODO(phoglund): should arguably be checked.
                  r'^tools[\\\/]valgrind-webrtc[\\\/].*\.py$',
                  r'^tools[\\\/]valgrind[\\\/].*\.py$',
                  r'^tools[\\\/]win[\\\/].*\.py$',
                  r'^xcodebuild.*[\\\/].*\.py$',),
      disabled_warnings=['F0401',  # Failed to import x
                         'E0611',  # No package y in x
                         'W0232',  # Class has no __init__ method
                        ],
      pylintrc='pylintrc'))

  # WebRTC can't use the presubmit_canned_checks.PanProjectChecks function since
  # we need to have different license checks in talk/ and webrtc/ directories.
  # Instead, hand-picked checks are included below.

  # .m and .mm files are ObjC files. For simplicity we will consider .h files in
  # ObjC subdirectories ObjC headers.
  objc_filter_list = (r'.+\.m$', r'.+\.mm$', r'.+objc\/.+\.h$')
  # Skip long-lines check for DEPS, GN and GYP files.
  build_file_filter_list = (r'.+\.gyp$', r'.+\.gypi$', r'.+\.gn$', r'.+\.gni$',
      'DEPS')
  eighty_char_sources = lambda x: input_api.FilterSourceFile(x,
      black_list=build_file_filter_list + objc_filter_list)
  hundred_char_sources = lambda x: input_api.FilterSourceFile(x,
      white_list=objc_filter_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, maxlen=80, source_file_filter=eighty_char_sources))
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, maxlen=100,
      source_file_filter=hundred_char_sources))

  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeTodoHasOwner(
      input_api, output_api))
  results.extend(_CheckNativeApiHeaderChanges(input_api, output_api))
  results.extend(_CheckNoIOStreamInHeaders(input_api, output_api))
  results.extend(_CheckNoFRIEND_TEST(input_api, output_api))
  results.extend(_CheckGypChanges(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(_CheckJSONParseErrors(input_api, output_api))
  results.extend(_RunPythonTests(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckGNFormatted(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(_VerifyNativeApiHeadersListIsValid(input_api, output_api))
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeWasUploaded(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api,
      json_url='http://webrtc-status.appspot.com/current?format=json'))
  return results


# pylint: disable=W0613
def GetPreferredTryMasters(project, change):
  cq_config_path = os.path.join(
      change.RepositoryRoot(), 'infra', 'config', 'cq.cfg')
  # commit_queue.py below is a script in depot_tools directory, which has a
  # 'builders' command to retrieve a list of CQ builders from the CQ config.
  is_win = platform.system() == 'Windows'
  masters = json.loads(subprocess.check_output(
      ['commit_queue', 'builders', cq_config_path], shell=is_win))

  try_config = {}
  for master in masters:
    try_config.setdefault(master, {})
    for builder in masters[master]:
      if 'presubmit' in builder:
        # Do not trigger presubmit builders, since they're likely to fail
        # (e.g. OWNERS checks before finished code review), and we're running
        # local presubmit anyway.
        pass
      else:
        try_config[master][builder] = ['defaulttests']

  return try_config
