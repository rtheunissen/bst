module.exports = {
    multipass: true,
    plugins: [
        'preset-default',
        {
            name: 'prefixIds',
            params: {
                prefix: function (ast, params, info) {
                    return require('crypto').createHash('md5').update(params.path).digest('hex').substring(0, 5);
                }
            },
        },
    ],
};