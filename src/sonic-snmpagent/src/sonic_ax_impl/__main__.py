from signal import signal, SIGUSR1
import logging.handlers
import os
import shutil
import sys

import swsssdk.util

import ax_interface
import sonic_ax_impl
from . import mibs

LOG_FORMAT = "snmp-subagent [%(name)s] %(levelname)s: %(message)s"

# set signal handlers to switch log level
def signal_handler_sigusr1(signal, frame):
    # set log level to debug or revert if set already
    if sonic_ax_impl.logger.getEffectiveLevel() != logging.DEBUG:
        sonic_ax_impl.logger.info("signal_handler_sigusr1(): Setting logger level to debug")
        sonic_ax_impl.logger.setLevel(logging.DEBUG)
        ax_interface.logger.setLevel(logging.DEBUG)
    else:
        sonic_ax_impl.logger.info("signal_handler_sigusr1(): Revert logger level to info")
        sonic_ax_impl.logger.setLevel(logging.INFO)
        ax_interface.logger.setLevel(logging.INFO)
    return

signal(SIGUSR1, signal_handler_sigusr1)
# end signal handlers

def install_file(src_filename, dest_dir, executable=False):
    dest_file = shutil.copy(src_filename, dest_dir)
    print("copied: ", dest_file)
    if executable:
        print("chmod +x {}".format(dest_file))
        st = os.stat(dest_file)
        os.chmod(dest_file, st.st_mode | 0o111)


def install_fragments():
    local_filepath = os.path.dirname(os.path.abspath(__file__))
    pass_script = os.path.join(local_filepath, 'bin/sysDescr_pass.py')
    install_file(pass_script, '/usr/share/snmp', executable=True)


if __name__ == "__main__":

    if 'install' in sys.argv:
        install_fragments()
        sys.exit(0)

    # import command line arguments
    args = swsssdk.util.process_options("sonic_ax_impl")

    # configure logging. If debug '-d' is specified, logs to stdout at designated level. syslog/INFO otherwise.
    log_level = log_level_sdk = args.get('log_level')
    warn_syslog = False
    if log_level is None:
        try:
            logging_handler = logging.handlers.SysLogHandler(address='/dev/log',
                                                             facility=logging.handlers.SysLogHandler.LOG_DAEMON)
        except (AttributeError, OSError):
            # when syslog is unavailable, log to stderr
            logging_handler = logging.StreamHandler(sys.stderr)
            warn_syslog = True

        logging_handler.setFormatter(logging.Formatter(LOG_FORMAT))
        sonic_ax_impl.logger.addHandler(logging_handler)
        log_level = logging.INFO
        log_level_sdk = logging.ERROR
    else:
        sonic_ax_impl.logger.addHandler(logging.StreamHandler(sys.stdout))

    # set the log levels
    sonic_ax_impl.logger.setLevel(log_level)
    ax_interface.logger.setLevel(log_level)
    swsssdk.logger.setLevel(log_level_sdk)

    # inherit logging handlers in submodules
    ax_interface.logger.handlers = sonic_ax_impl.logger.handlers
    swsssdk.logger.handlers = sonic_ax_impl.logger.handlers

    # init mibs module with command line arguments
    mibs.config(**args)

    if warn_syslog:
        # syslog was unavailable when it should've been.
        sonic_ax_impl.logger.warning("Syslog is unavailable. Logging to STDERR.")

    from .main import main

    main(update_frequency=args.get('update_frequency'))
