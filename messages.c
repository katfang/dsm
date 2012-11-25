client_id_t
lowest_id(copyset_t id_set)
{
  if (id_set) {
    int d;
    for(d = 0; d < 64; d++) {
      if (id_set & (1 << d)) {
	return d + 1;
      }
    }
  }
  return 0;
}
