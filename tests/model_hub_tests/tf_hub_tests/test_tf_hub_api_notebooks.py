# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import pytest
import tensorflow as tf
import tensorflow_hub as hub
from models_hub_common.test_convert_model import TestConvertModel
from tf_hub_tests.utils import get_input_info


class TestTFHubApiNotebooks(TestConvertModel):
    def load_model(self, model_name, model_link):
        if model_name == 'mobilenet_v2_100_224_dict':
            image = tf.keras.layers.Input(shape=(224, 224, 3), dtype=tf.float32, name="image")
            feature_vector = hub.KerasLayer("https://tfhub.dev/google/imagenet/mobilenet_v2_100_224/feature_vector/5",
                                            trainable=False)(image)
            softmax = tf.keras.layers.Dense(20, activation='softmax')(feature_vector)
            classification_model = tf.keras.Model(inputs={'image': image}, outputs={'softmax': softmax})
            return classification_model
        elif model_name == 'mobilenet_v2_100_224_list':
            image = tf.keras.layers.Input(shape=(224, 224, 3), dtype=tf.float32, name="image")
            feature_vector = hub.KerasLayer("https://tfhub.dev/google/imagenet/mobilenet_v2_100_224/feature_vector/5",
                                            trainable=False)(image)
            softmax = tf.keras.layers.Dense(20, activation='softmax')(feature_vector)
            classification_model = tf.keras.Model(inputs=[image], outputs=[softmax])
            return classification_model
        else:
            raise "Unknown input model: {}".format(model_name)

    def get_inputs_info(self, keras_model):
        inputs_info = []
        if isinstance(keras_model.input, dict):
            for input_name, input_tensor in keras_model.input.items():
                inputs_info.append(get_input_info(input_tensor, input_name))
        elif isinstance(keras_model.input, list):
            inputs_info.append('list')
            for input_tensor in keras_model.input:
                inputs_info.append(get_input_info(input_tensor, input_tensor.name))
        else:
            inputs_info.append('list')
            input_tensor = keras_model.input
            inputs_info.append(get_input_info(input_tensor, input_tensor.name))
        return inputs_info

    def infer_fw_model(self, model_obj, inputs):
        outputs = model_obj(inputs)
        if isinstance(outputs, dict):
            post_outputs = {}
            for out_name, out_value in outputs.items():
                post_outputs[out_name] = out_value.numpy()
        elif isinstance(outputs, list):
            post_outputs = []
            for out_value in outputs:
                post_outputs.append(out_value.numpy())
        else:
            post_outputs = [outputs.numpy()]

        return post_outputs

    @pytest.mark.precommit
    @pytest.mark.parametrize("model_name", ['mobilenet_v2_100_224_dict', 'mobilenet_v2_100_224_list'])
    def test_tf_hub_api_notebook1(self, model_name, ie_device):
        self.run(model_name, '', ie_device)
