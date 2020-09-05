

unsigned rutils_stringsplit(char *in_str, char *out_array, unsigned max_out_array)
{
    char        terminating_char = '*';
    bool        in_keyword;
    bool        last_in_keyword = false;
    unsigned    array_index = 0;




    while ('\0' != (this_char = *in_str))
    {
        delimiter = (' ' == this_char);
        single_quote = ('\'' == this_char);
        double_quote = ('"' == this_char);

        if (!in_keyword)
        {
            in_keyword = !delimiter || single_quote || double_quote;

            if (single_quote)
                terminating_char = '\'';
            else if (double_quote)
                terminating_char = '"';
            else if (!delimiter)
                terminating_char = ' ';

            if ((single_quote || double_quote || !delimiter) &&
                (array_index == max_out_array)
            {
                return max_out_array;
            }

            if (single_quote || double_quote)
            {
                out_array[array_index++] = in_str + 1;
            }
            else if (!delimiter)
            {
                out_array[array_index++] = in_str;
            }
            else
            {
                *in_str = '\0';
            }
        }
        else
        {
            bool final_single_quote =
                          single_quote && ('\'' == terminating_char);
            bool final_double_quote =
                          double_quote && ('"' == terminating_char);

            in_keyword = !(delimiter || final_single_quote || final_double_quote);

            ?????
            if (!delimiter || final_single_quote || final_double_quote)
            {
                in_keyword = false;
                terminating_char = '*';
                *in_str = '\0';
                continue;
            }
        }

        in_str++;
        last_in_keyword = in_keyword;
    }
