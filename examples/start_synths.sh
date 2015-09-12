#/usr/bin/env sh
SYNTH="pd -jack -nogui synth.pd"
TMPDIR=/tmp/
INSTANCES=${1-$(nproc)}

echo starting $INSTANCES synths ...

for n in $(seq 1 $INSTANCES); do
    $SYNTH &> /dev/null &
    echo $! > "$TMPDIR/_synth_$n.pid"
done

echo press any key to stop.
read

for n in $(seq 1 $INSTANCES); do
    kill $(cat "$TMPDIR/_synth_$n.pid")
done
