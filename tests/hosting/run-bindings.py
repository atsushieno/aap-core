#!/usr/bin/env python3
"""Run the actual connector against JVM platform doubles, without changing Gradle."""
from pathlib import Path
import argparse, os, re, subprocess, sys, tempfile, platform

arguments = argparse.ArgumentParser(description=__doc__)
arguments.add_argument("--api-baseline", help="Git revision to compare public JVM and Kotlin contracts against")
options = arguments.parse_args()

repo = Path(__file__).resolve().parents[2]
cache = Path(os.environ.get('GRADLE_USER_HOME', str(Path.home() / '.gradle'))) / 'caches/modules-2/files-2.1'
versions = (repo / 'gradle/libs.versions.toml').read_text()
def version(name):
    return re.search(r'^' + name + r' = "([^"]+)"', versions, re.M).group(1)
def jar(group, name, release):
    matches = list((cache / group / name / release).glob('*/' + name + '-' + release + '.jar'))
    if len(matches) != 1:
        sys.exit('Missing cached ' + name + ': run ./gradlew :androidaudioplugin:compileDebugKotlin first.')
    return str(matches[0])
kotlin = version('kotlin')
stdlib = jar('org.jetbrains.kotlin', 'kotlin-stdlib', kotlin)
coroutines = jar('org.jetbrains.kotlinx', 'kotlinx-coroutines-core-jvm', version('coroutines'))
compiler = [jar('org.jetbrains.kotlin', name, kotlin) for name in
            ('kotlin-compiler-embeddable', 'kotlin-script-runtime', 'kotlin-reflect')]
compiler += [stdlib, coroutines]
annotations = list((cache / 'org.jetbrains/annotations').glob('*/*/annotations-*.jar'))
compiler += [str(p) for p in annotations if not p.name.endswith('-sources.jar')][:1]
settings = subprocess.run(['java', '-XshowSettings:properties', '-version'], capture_output=True, text=True, check=True)
java_home = Path(re.search(r'java.home = (.+)', settings.stderr).group(1).strip())
with tempfile.TemporaryDirectory(prefix='aap-binding-tests-') as temporary:
    build = Path(temporary)
    native = build / ('notifications.dylib' if platform.system() == 'Darwin' else 'notifications.so')
    subprocess.run([os.environ.get('CC', 'cc'), '-shared', '-fPIC', '-I' + str(java_home / 'include'),
                    '-I' + str(java_home / 'include' / ('darwin' if platform.system() == 'Darwin' else 'linux')),
                    str(repo / 'tests/hosting/notifications.c'), '-o', str(native)], check=True)
    sources = sorted((repo / 'tests/hosting/stubs').glob('*.kt'))
    sources += [repo / 'androidaudioplugin/src/main/java/org/androidaudioplugin/hosting' / name
                for name in ('AudioPluginServiceConnector.kt', 'PluginServiceConnection.kt')]
    surface = repo / 'tests/hosting/surface.kt'
    sources += [repo / 'tests/hosting/bindings.kt', surface]
    classes = build / 'classes'
    reflection = jar('org.jetbrains.kotlin', 'kotlin-reflect', kotlin)
    classpath = os.pathsep.join([stdlib, coroutines, reflection])
    def compile_to(destination, files):
        subprocess.run(['java', '-cp', os.pathsep.join(compiler), 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                        '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', classpath,
                        '-d', str(destination), *map(str, files)], check=True)
    compile_to(classes, sources)
    if options.api_baseline:
        production = repo / 'androidaudioplugin/src/main/java/org/androidaudioplugin/hosting/AudioPluginServiceConnector.kt'
        baseline_source = build / 'AudioPluginServiceConnector.kt'
        baseline_source.write_bytes(subprocess.check_output(
            ['git', 'show', options.api_baseline + ':' + str(production.relative_to(repo))], cwd=repo))
        baseline_classes = build / 'baseline-classes'
        compile_to(baseline_classes, [baseline_source if p == production else p for p in sources])
        def surface_at(directory):
            result = subprocess.check_output(['java', '-cp', os.pathsep.join([str(directory), classpath]),
                                              'org.androidaudioplugin.hosting.SurfaceKt'], text=True)
            for name in ('AudioPluginServiceConnector', 'AudioPluginServiceConnector$Connection',
                         'AudioPluginServiceConnector$Companion'):
                bytecode = subprocess.check_output(['javap', '-classpath', str(directory), '-public', '-s',
                    'org.androidaudioplugin.hosting.' + name], text=True).splitlines()
                skip_descriptor = False
                for line in bytecode:
                    if ' access$' in line:
                        skip_descriptor = True
                    elif skip_descriptor and 'descriptor:' in line:
                        skip_descriptor = False
                    elif line.strip():
                        result += line + '\n'
            return result
        before, after = surface_at(baseline_classes), surface_at(classes)
        if before != after:
            import difflib
            sys.exit(''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                                 fromfile='baseline API', tofile='current API')))
        print('PASS: public Kotlin contracts and JVM descriptors match ' + options.api_baseline, flush=True)
    subprocess.run(['java', '-cp', os.pathsep.join([str(classes), stdlib, coroutines]),
                    'org.androidaudioplugin.hosting.BindingsKt', str(native)], check=True)
