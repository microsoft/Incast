# Incast

This is a client-server network benchmark that simulates the “incast” traffic pattern which is common to map-reduce style cloud apps.

A server sends a “fan-out” request to many clients, which respond simultaneously with a “fan-in” response.  The response is the incast event.

This benchmark allows someone using Windows to easily create this traffic pattern in order to evaluate the performance of switches, NICs, and other networking gear when faced with the incast pattern.
